// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KEY_IO_H
#define BITCOIN_KEY_IO_H

#include <electrumz/bitcoin/script.h>

#include <string>

std::string EncodeDestination(const CTxDestination& dest);

#endif // BITCOIN_KEY_IO_H
