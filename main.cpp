// Copyright (c) 2011-2012, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gelf.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/relaxng.h>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <memory>
#include <climits>
#include <set>
#include <map>

#include "Trace.h"
#include "Stats.h"
#include "Resource.h"
#include "Core.h"
#include "SyscallHandler.h"
#include "SymbolInfo.h"
#include "XE.h"
#include "SE.h"
#include "ScopedArray.h"
#include "Config.h"
#include "Instruction.h"
#include "Node.h"
#include "SystemState.h"
#include "LatencyModel.h"

#define XCORE_ELF_MACHINE 0xB49E

const char configSchema[] =
#include "ConfigSchema.inc"
;

static void printUsage(const char *ProgName) {
  std::cout << "Usage: " << ProgName << " [options] filename\n";
  std::cout <<
"General Options:\n"
"  -h Display this information.\n"
"  -c Specify a configuration file.\n"
"  -C Specify a configuration file and display values.\n"
"  -t Enable instruction tracing.\n"
"  -s Simulate a se program.\n"
"  -S Display system statistics.\n"
"  -I Display instruction statistics.\n"
"\n";
}

static void readSymbols(Elf *e, Elf_Scn *scn, const GElf_Shdr &shdr,
                        unsigned low, unsigned high,
                        std::auto_ptr<CoreSymbolInfo> &SI)
{
  Elf_Data *data = elf_getdata(scn, NULL);
  if (data == NULL) {
    return;
  }
  unsigned count = shdr.sh_size / shdr.sh_entsize;

  CoreSymbolInfoBuilder builder;

  for (unsigned i = 0; i < count; i++) {
    GElf_Sym sym;
    if (gelf_getsym(data, i, &sym) == NULL) {
      continue;
    }
    if (sym.st_shndx == SHN_ABS)
      continue;
    if (sym.st_value < low || sym.st_value >= high)
      continue;
    builder.addSymbol(elf_strptr(e, shdr.sh_link, sym.st_name),
                      sym.st_value,
                      sym.st_info);
  }
  SI = builder.getSymbolInfo();
}

static void readSymbols(Elf *e, unsigned low, unsigned high,
                        std::auto_ptr<CoreSymbolInfo> &SI)
{
  Elf_Scn *scn = NULL;
  GElf_Shdr shdr;
  while ((scn = elf_nextscn(e, scn)) != NULL) {
    if (gelf_getshdr(scn, &shdr) == NULL) {
      continue;
    }
    if (shdr.sh_type == SHT_SYMTAB) {
      // Found the symbol table
      break;
    }
  }
  
  if (scn != NULL) {
    readSymbols(e, scn, shdr, low, high, SI);
  }
}

static xmlNode *findChild(xmlNode *node, const char *name)
{
  for (xmlNode *child = node->children; child; child = child->next) {
    if (child->type != XML_ELEMENT_NODE)
      continue;
    if (strcmp(name, (char*)child->name) == 0)
      return child;
  }
  return 0;
}

static xmlAttr *findAttribute(xmlNode *node, const char *name)
{
  for (xmlAttr *child = node->properties; child; child = child->next) {
    if (child->type != XML_ATTRIBUTE_NODE)
      continue;
    if (strcmp(name, (char*)child->name) == 0)
      return child;
  }
  return 0;
}

static void readElf(const char *filename, const XEElfSector *elfSector,
                    Core &core, std::auto_ptr<CoreSymbolInfo> &SI,
                    std::map<Core*,uint32_t> &entryPoints)
{
  uint64_t ElfSize = elfSector->getElfSize();
  const scoped_array<char> buf(new char[ElfSize]);
  if (!elfSector->getElfData(buf.get())) {
    std::cerr << "Error reading elf data from \"" << filename << "\"" << std::endl;
    std::exit(1);
  }
  
  if (elf_version(EV_CURRENT) == EV_NONE) {
    std::cerr << "ELF library intialisation failed: "
              << elf_errmsg(-1) << std::endl;
    std::exit(1);
  }
  Elf *e;
  if ((e = elf_memory(buf.get(), ElfSize)) == NULL) {
    std::cerr << "Error reading ELF: " << elf_errmsg(-1) << std::endl;
    std::exit(1);
  }
  if (elf_kind(e) != ELF_K_ELF) {
    std::cerr << filename << " is not an ELF object" << std::endl;
    std::exit(1);
  }
  GElf_Ehdr ehdr;
  if (gelf_getehdr(e, &ehdr) == NULL) {
    std::cerr << "Reading ELF header failed: " << elf_errmsg(-1) << std::endl;
    std::exit(1);
  }
  if (ehdr.e_machine != XCORE_ELF_MACHINE) {
    std::cerr << "Not a XCore ELF" << std::endl;
    std::exit(1);
  }
  if (ehdr.e_entry != 0) {
    entryPoints.insert(std::make_pair(&core, (uint32_t)ehdr.e_entry));
  }
  unsigned num_phdrs = ehdr.e_phnum;
  if (num_phdrs == 0) {
    std::cerr << "No ELF program headers" << std::endl;
    std::exit(1);
  }
  uint32_t ram_base = core.ram_base;
  uint32_t ram_size = core.ram_size;
  //std::cout<<"Ram base "<<std::hex<<ram_base<<", ram size "<<ram_size<<std::dec<<std::endl;
  for (unsigned i = 0; i < num_phdrs; i++) {
    GElf_Phdr phdr;
    if (gelf_getphdr(e, i, &phdr) == NULL) {
      std::cerr << "Reading ELF program header " << i << " failed: " << elf_errmsg(-1) << std::endl;
      std::exit(1);
    }
    if (phdr.p_filesz == 0) {
      continue;
    }
    //std::cout<<std::hex<<"p_paddr "<<phdr.p_paddr<<" p_memsz "<<phdr.p_memsz<<std::dec<<std::endl;
    if (phdr.p_offset > ElfSize) {
    	std::cerr << "Invalid offet in ELF program header" << i << std::endl;
    	std::exit(1);
    }
    uint32_t offset = phdr.p_paddr - core.ram_base;
    if (offset > ram_size || offset + phdr.p_filesz > ram_size || offset + phdr.p_memsz > ram_size) {
      std::cerr << "Error data from ELF program header " << i << " does not fit in memory" << std::endl;
      std::exit(1);
    }
    std::memcpy(core.mem() + offset, &buf[phdr.p_offset], phdr.p_filesz);
  }

  readSymbols(e, ram_base, ram_base + ram_size, SI);
  
  elf_end(e);
}

enum ProcessorState {
  PS_RAM_BASE = 0x00b,
  PS_VECTOR_BASE = 0x10b
};

enum {
  SETC_INUSE_OFF = 0x0,
  SETC_INUSE_ON = 0x8,
  SETC_COND_FULL = 0x1,
  SETC_COND_AFTER = 0x9,
  SETC_COND_EQ = 0x11,
  SETC_COND_NEQ = 0x19,
  SETC_IE_MODE_EVENT = 0x2,
  SETC_IE_MODE_INTERRUPT = 0xa,
  SETC_RUN_STOPR = 0x7,
  SETC_RUN_STARTR = 0xf,
  SETC_RUN_CLRBUF = 0x17,
  SETC_MS_MASTER = 0x1007,
  SETC_MS_SLAVE = 0x100f,
  SETC_BUF_NOBUFFERS = 0x2007,
  SETC_BUF_BUFFERS = 0x200f,
  SETC_RDY_NOREADY = 0x3007,
  SETC_RDY_STROBED = 0x300f,
  SETC_RDY_HANDSHAKE = 0x3017,
  SETC_PORT_DATAPORT = 0x5007,
  SETC_PORT_CLOCKPORT = 0x500f,
  SETC_PORT_READYPORT = 0x5017
};

static long readNumberAttribute(xmlNode *node, const char *name)
{
  xmlAttr *attr = findAttribute(node, name);
  assert(attr);
  errno = 0;
  long value = std::strtol((char*)attr->children->content, 0, 0);
  if (errno != 0) {
    std::cerr << "Invalid " << name << '"' << (char*)attr->children->content
              << "\"\n";
    exit(1);
  }
  return value;
}

static inline std::auto_ptr<Core>
createCoreFromConfig(xmlNode *config)
{
  uint32_t ram_size = RAM_SIZE;
  uint32_t ram_base = RAM_BASE;
  xmlNode *memoryController = findChild(config, "MemoryController");
  xmlNode *ram = findChild(memoryController, "Ram");
  ram_base = readNumberAttribute(ram, "base");
  ram_size = readNumberAttribute(ram, "size");
  std::auto_ptr<Core> core(new Core(ram_size, ram_base));
  core->setCoreNumber(readNumberAttribute(config, "number"));
  //if (xmlAttr *codeReference = findAttribute(config, "codeReference")) {
  //  core->setCodeReference((char*)codeReference->children->content);
  //}
  return core;
}

static inline std::auto_ptr<Node>
createNodeFromConfig(xmlNode *config,
                     std::map<long,Node*> &nodeNumberMap)
{
  long jtagID = readNumberAttribute(config, "jtagId");
  Node::Type nodeType;
  if (!Node::getTypeFromJtagID(jtagID, nodeType)) {
    std::cerr << "Unknown jtagId 0x" << std::hex << jtagID << std::dec << '\n';
    std::exit(1);
  }
  std::auto_ptr<Node> node(new Node(nodeType));
  long nodeID = readNumberAttribute(config, "number");
  nodeNumberMap.insert(std::make_pair(nodeID, node.get()));
  for (xmlNode *child = config->children; child; child = child->next) {
    if (child->type != XML_ELEMENT_NODE ||
        strcmp("Processor", (char*)child->name) != 0)
      continue;
    node->addCore(createCoreFromConfig(child));
  }
  node->setNodeID(nodeID);
  return node;
}

static inline std::auto_ptr<SystemState>
createSystemFromConfig(const char *filename, const XESector *configSector)
{
  uint64_t length = configSector->getLength();
  const scoped_array<char> buf(new char[length + 1]);
  if (!configSector->getData(buf.get())) {
    std::cerr << "Error reading config from \"" << filename << "\"" << std::endl;
    std::exit(1);
  }
  if (length < 8) {
    std::cerr << "Error unexpected config config sector length" << std::endl;
    std::exit(1);
  }
  length -= 8;
  buf[length] = '\0';
  
  /*
   * this initialize the library and check potential ABI mismatches
   * between the version it was compiled for and the actual shared
   * library used.
   */
  LIBXML_TEST_VERSION
  
  xmlDoc *doc = xmlReadDoc((xmlChar*)buf.get(), "config.xml", NULL, 0);

  xmlRelaxNGParserCtxtPtr schemaContext =
    xmlRelaxNGNewMemParserCtxt(configSchema, sizeof(configSchema));
  xmlRelaxNGPtr schema = xmlRelaxNGParse(schemaContext);
  xmlRelaxNGValidCtxtPtr validationContext =
    xmlRelaxNGNewValidCtxt(schema);
  if (xmlRelaxNGValidateDoc(validationContext, doc) != 0) {
    std::exit(1);
  }

  xmlNode *root = xmlDocGetRootElement(doc);
  xmlNode *system = findChild(root, "System");
  xmlNode *nodes = findChild(system, "Nodes");
  std::auto_ptr<SystemState> systemState(new SystemState);
  std::map<long,Node*> nodeNumberMap;
  for (xmlNode *child = nodes->children; child; child = child->next) {
    if (child->type != XML_ELEMENT_NODE ||
        strcmp("Node", (char*)child->name) != 0)
      continue;
    systemState->addNode(createNodeFromConfig(child, nodeNumberMap));
  }
  xmlNode *jtag = findChild(system, "JtagChain");
  unsigned jtagIndex = 0;
  for (xmlNode *child = jtag->children; child; child = child->next) {
    if (child->type != XML_ELEMENT_NODE ||
        strcmp("Node", (char*)child->name) != 0)
      continue;
    long nodeID = readNumberAttribute(child, "id");
    std::map<long,Node*>::iterator it = nodeNumberMap.find(nodeID);
    if (it == nodeNumberMap.end()) {
      std::cerr << "No node matching id " << nodeID << std::endl;
      std::exit(1);
    }
    it->second->setJtagIndex(jtagIndex++);
  }
  xmlFreeDoc(doc);
  xmlCleanupParser();
  return systemState;
}

static inline void
addToCoreMap(std::map<std::pair<unsigned, unsigned>,Core*> &coreMap,
             Node &node)
{
  unsigned jtagIndex = node.getJtagIndex();
  const std::vector<Core*> &cores = node.getCores();
  unsigned coreNum = 0;
  for (std::vector<Core*>::const_iterator it = cores.begin(), e = cores.end();
       it != e; ++it) {
    coreMap.insert(std::make_pair(std::make_pair(jtagIndex, coreNum), *it));
    coreNum++;
  }
}


static inline void
addToCoreMap(std::map<std::pair<unsigned, unsigned>,Core*> &coreMap,
             SystemState &system)
{
  for (SystemState::node_iterator it = system.node_begin(),
       e = system.node_end(); it != e; ++it) {
    addToCoreMap(coreMap, **it);
  }
}

static inline std::auto_ptr<SystemState>
readXE(const char *filename, SymbolInfo &SI,
       std::set<Core*> &coresWithImage, std::map<Core*,uint32_t> &entryPoints)
{
  // Load the file into memory.
  XE xe(filename);
  if (!xe) {
    std::cerr << "Error opening \"" << filename << "\"" << std::endl;
    std::exit(1);
  }
  xe.read();
  // TODO handle XEs / XBs without a config sector.
  const XESector *configSector = xe.getConfigSector();
  if (!configSector) {
    std::cerr << "Error: No config file found in \"" << filename << "\"" << std::endl;
    std::exit(1);
  }
  std::auto_ptr<SystemState> system =
    createSystemFromConfig(filename, configSector);
  std::map<std::pair<unsigned, unsigned>,Core*> coreMap;
  addToCoreMap(coreMap, *system);
  for (std::vector<const XESector *>::const_reverse_iterator
       it = xe.getSectors().rbegin(), end = xe.getSectors().rend(); it != end;
       ++it) {
    switch((*it)->getType()) {
    case XESector::XE_SECTOR_ELF:
      {
        const XEElfSector *elfSector = static_cast<const XEElfSector*>(*it);
        unsigned jtagIndex = elfSector->getNode();
        unsigned coreNum = elfSector->getCore();
        Core *core = coreMap[std::make_pair(jtagIndex, coreNum)];
        if (!core) {
          std::cerr << "Error: cannot find node " << jtagIndex
                    << ", core " << coreNum << std::endl;
          std::exit(1);
        }
        if (coresWithImage.count(core))
          continue;
        std::auto_ptr<CoreSymbolInfo> CSI;
        readElf(filename, elfSector, *core, CSI, entryPoints);
        SI.add(core, CSI);
        coresWithImage.insert(core);
        break;
      }
    }
  }
  xe.close();
  return system;
}

static inline std::auto_ptr<SystemState>
createSESystem(const char *filename, int numCores)
{
  std::auto_ptr<SystemState> systemState(new SystemState());
  std::map<long, Node*> nodeNumberMap;

  // Create a single parent node
  std::auto_ptr<Node> node(new Node(Node::XS1_L));
  unsigned jtagIndex = 0;
  node->setJtagIndex(jtagIndex);
  nodeNumberMap.insert(std::make_pair(jtagIndex, node.get()));

  // Create child cores
  for (int i=0; i<numCores; i++) {
    std::auto_ptr<Core> core(new Core(RAM_SIZE, RAM_BASE));
    core->setCoreNumber(i);
    node->addCore(core);
    //std::cout<<"Created core "<<i<<"\n";
  }

  node->setNodeID(jtagIndex);
  systemState->addNode(node);
  return systemState;
}

static inline std::auto_ptr<SystemState>
readSE(const char *filename, SymbolInfo &SI, std::set<Core*> &coresWithImage, 
    std::map<Core*,uint32_t> &entryPoints)
{
  //std::cout << "Reading " << filename << std::endl;
  
  // Load the file into memory.
  SE se(filename);
  if (!se) {
    std::cerr << "Error opening \"" << filename << "\"" << std::endl;
    std::exit(1);
  }
  se.read();

  // Create the system
  std::auto_ptr<SystemState> system = createSESystem(filename, se.getNumCores());
  std::map<std::pair<unsigned, unsigned>, Core*> coreMap;
  addToCoreMap(coreMap, *system);

  // Pick out the master and slave Elf sectors
  const XEElfSector *masterElfSector = NULL;
  const XEElfSector *slaveElfSector = NULL;
  for (std::vector<const XESector *>::const_reverse_iterator
       it = se.getSectors().rbegin(), end = se.getSectors().rend(); it != end;
       ++it) {
    switch((*it)->getType()) {
    case XESector::XE_SECTOR_ELF:
      {
        const XEElfSector *elfSector = static_cast<const XEElfSector*>(*it);
        unsigned coreNum = elfSector->getCore();
        if (coreNum == 0)
          masterElfSector = static_cast<const XEElfSector*>(*it);
        else if (coreNum == 1) 
          slaveElfSector = static_cast<const XEElfSector*>(*it);
      }
    }
  }

  // Load the master and slave Elf images
  for(int i=0; i<se.getNumCores(); i++) {
    unsigned jtagIndex = 0;
    Core *core = coreMap[std::make_pair(jtagIndex, i)];
    if (!core) {
      std::cerr << "Error: cannot find core " << i << std::endl;
      std::exit(1);
    }
    std::auto_ptr<CoreSymbolInfo> CSI;
    readElf(filename, i==0 ? masterElfSector : slaveElfSector, *core, CSI, entryPoints);
    SI.add(core, CSI);
    coresWithImage.insert(core);
    //std::cout<<"Loaded core "<<core->getCoreID()<<"\n";
  }

  se.close();
  return system;
}

int loop(const char *filename, bool tracing, bool se, 
    bool systemStats, bool instStats) {
  std::auto_ptr<SymbolInfo> SI(new SymbolInfo);
  std::set<Core*> coresWithImage;
  std::map<Core*,uint32_t> entryPoints;
  std::auto_ptr<SystemState> statePtr = se ?
    readSE(filename, *SI, coresWithImage, entryPoints) :
    readXE(filename, *SI, coresWithImage, entryPoints);
  SystemState &sys = *statePtr;

  for (std::set<Core*>::iterator it = coresWithImage.begin(),
       e = coresWithImage.end(); it != e; ++it) {
    Core *core = *it;
    sys.schedule(core->getThread(0));

    // Patch in syscall instruction at the syscall address.
    if (const ElfSymbol *syscallSym = SI->getGlobalSymbol(core, "_DoSyscall")) {
      if (!core->setSyscallAddress(syscallSym->value)) {
        std::cout << "Warning: invalid _DoSyscall address "
        << std::hex << syscallSym->value << std::dec << "\n";
      }
    }
    // Patch in exception instruction at the exception address
    if (const ElfSymbol *doExceptionSym = SI->getGlobalSymbol(core, "_DoException")) {
      if (!core->setExceptionAddress(doExceptionSym->value)) {
        std::cout << "Warning: invalid _DoException address "
        << std::hex << doExceptionSym->value << std::dec << "\n";
      }
    }
    std::map<Core*,uint32_t>::iterator match;
    if ((match = entryPoints.find(core)) != entryPoints.end()) {
      uint32_t entryPc = core->physicalAddress(match->second) >> 1;
      if (entryPc< core->ram_size << 1) {
        core->getThread(0).pc = entryPc;
      } else {
        std::cout << "Warning: invalid ELF entry point 0x";
        std::cout << std::hex << match->second << std::dec << "\n";
      }
    }
  }
  SyscallHandler::setCoreCount(coresWithImage.size());
  LatencyModel::get().init(coresWithImage.size());
 
  // Inisialise instruction statistics
  if (instStats) {
    Stats::get().initStats(coresWithImage.size());
    Stats::get().setEnabled(true);
  }
 
  // Initialise tracing
  Tracer::get().setSymbolInfo(SI);
  if (tracing) {
    Tracer::get().setTracingEnabled(tracing);
  }

  // Run the simulation
  int status = sys.run();

  // Display statistics
  if (systemStats)
    sys.stats();
  if (instStats)
    Stats::get().dump();

  return status;
}

int
main(int argc, char **argv) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }
  const char *file = 0;
  bool tracing = false;
  bool se = false;
  bool systemStats = false;
  bool instStats = false;
  std::string arg;
  for (int i = 1; i < argc; i++) {
    arg = argv[i];
    if (arg == "-t") {
      tracing = true;
    } else if (arg == "-s") {
      se = true;
    } else if (arg == "-c") {
      if (i + 1 > argc) {
        printUsage(argv[0]);
        return 1;
      }
      if(!Config::get().read(argv[i + 1])) 
        return 1;
      i++;
    } else if (arg == "-C") {
      if (i + 1 > argc) {
        printUsage(argv[0]);
        return 1;
      }
      if(!Config::get().read(argv[i + 1])) 
        return 1;
      Config::get().display();
      i++;
    } else if (arg == "-S") {
      systemStats = true;
    } else if (arg == "-I") {
      instStats = true;
    } else if (arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else {
      if (file) {
        printUsage(argv[0]);
        return 1;
      }
      file = argv[i];
    }
  }
  if (!file) {
    printUsage(argv[0]);
    return 1;
  }
#ifndef _WIN32
  if (isatty(fileno(stdout))) {
    Tracer::get().setColour(true);
  }
#endif
  return loop(file, tracing, se, systemStats, instStats);
}
