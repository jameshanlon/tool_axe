// Copyright (c) 2011, Richard Osborne, All rights reserved
// This software is freely distributable under a derivative of the
// University of Illinois/NCSA Open Source License posted in
// LICENSE.txt and at <http://github.xcore.com/>


#ifndef _UartRx_h_
#define _UartRx_h_

#include <memory>

class PeripheralDescriptor;

std::auto_ptr<PeripheralDescriptor> getPeripheralDescriptorUartRx();

#endif // _UartRx_h_
