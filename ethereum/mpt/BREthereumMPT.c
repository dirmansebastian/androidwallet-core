//
//  BREthereumMPT.c
//  Core
//
//  Created by Ed Gamble on 8/21/18.
//  Copyright (c) 2018 breadwallet LLC
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "BREthereumMPT.h"

extern BREthereumMPTNodePath
mptProofDecode (BRRlpItem item,
                BRRlpCoder coder) {
    rlpShowItem (coder, item, "MPT Proof");
    return (BREthereumMPTNodePath) {};
}

extern BRArrayOf(BREthereumMPTNodePath)
mptProofDecodeList (BRRlpItem item,
                    BRRlpCoder coder) {
    size_t itemsCount;
    const BRRlpItem *items = rlpDecodeList (coder, item, &itemsCount);

    // TODO: Wrong - here to flag success!
    assert (itemsCount == 0);
    
    BRArrayOf (BREthereumMPTNodePath) proofs;
    array_new (proofs, itemsCount);
    for (size_t index = 0; index < itemsCount; index++)
        array_add (proofs, mptProofDecode (items[index], coder));
    return proofs;
}