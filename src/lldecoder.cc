/**
 * This file is part of Rellume.
 *
 * (c) 2019, Alexis Engelke <alexis.engelke@googlemail.com>
 *
 * Rellume is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License (LGPL)
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Rellume is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Rellume.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#include <deque>
#include <iostream>
#include <unordered_map>
#include <vector>

#include <rellume/rellume.h>

#include <rellume/instr.h>
#include <function.h>

namespace rellume {

#define instrIsJcc(instr) ( \
    (instr) == LL_INS_JO || \
    (instr) == LL_INS_JNO || \
    (instr) == LL_INS_JC || \
    (instr) == LL_INS_JNC || \
    (instr) == LL_INS_JZ || \
    (instr) == LL_INS_JNZ || \
    (instr) == LL_INS_JBE || \
    (instr) == LL_INS_JA || \
    (instr) == LL_INS_JS || \
    (instr) == LL_INS_JNS || \
    (instr) == LL_INS_JP || \
    (instr) == LL_INS_JNP || \
    (instr) == LL_INS_JL || \
    (instr) == LL_INS_JGE || \
    (instr) == LL_INS_JLE || \
    (instr) == LL_INS_JG \
)
#define instrBreaks(instr) (instrIsJcc(instr) || (instr) == LL_INS_RET || \
                        (instr) == LL_INS_JMP || (instr) == LL_INS_CALL)

int Function::Decode(uintptr_t addr)
{
    LLInstr inst;

    std::deque<uintptr_t> addr_queue;
    addr_queue.push_back(addr);

    std::vector<LLInstr> insts;
    // List of (start_idx,end_idx) (non-inclusive end)
    std::vector<std::pair<size_t,size_t>> blocks;

    // Mapping from address to (block_idx, instr_idx)
    std::unordered_map<uintptr_t, std::pair<size_t,size_t>> addr_map;

    std::deque<uintptr_t>::iterator addr_it = addr_queue.begin();
    while (addr_it != addr_queue.end())
    {
        uintptr_t cur_addr = *addr_it;
        addr_it = addr_queue.erase(addr_it);

        size_t cur_block_start = insts.size();

        auto cur_addr_entry = addr_map.find(cur_addr);
        while (cur_addr_entry == addr_map.end())
        {
            inst = LLInstr::Decode(reinterpret_cast<uint8_t*>(cur_addr), 15, cur_addr);
            // If we reach an invalid instruction or an instruction we can't
            // decode, stop.
            if (inst.type == LL_INS_Invalid)
                break;

            addr_map[cur_addr] = std::make_pair(blocks.size(), insts.size());
            insts.push_back(inst);
            if (instrBreaks(inst.type))
            {
                if (instrIsJcc(inst.type) || inst.type == LL_INS_CALL)
                    addr_queue.push_back(cur_addr + inst.len);
                if (instrIsJcc(inst.type) || inst.type == LL_INS_JMP)
                    addr_queue.push_back(inst.ops[0].val);
                break;
            }
            cur_addr += inst.len;
            cur_addr_entry = addr_map.find(cur_addr);
        }

        if (insts.size() != cur_block_start)
            blocks.push_back(std::make_pair(cur_block_start, insts.size()));

        if (cur_addr_entry != addr_map.end())
        {
            auto& other_blk = blocks[cur_addr_entry->second.first];
            size_t split_idx = cur_addr_entry->second.second;
            if (other_blk.first == split_idx)
                continue;
            size_t end = other_blk.second;
            blocks.push_back(std::make_pair(split_idx, end));
            blocks[cur_addr_entry->second.first].second = split_idx;
            for (size_t j = split_idx; j < end; j++)
                addr_map[insts[j].addr] = std::make_pair(blocks.size()-1, j);
        }
    }

    for (auto it = blocks.begin(); it != blocks.end(); it++)
    {
        uint64_t block_addr = insts[it->first].addr;
        for (size_t j = it->first; j < it->second; j++)
            AddInst(block_addr, insts[j]);
    }

    return 0;
}

} // namespace
