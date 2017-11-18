// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017 The UltimateOnlineCash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "auxpow.h"
#include "primitives/block.h"
#include "uint256.h"

#include "util.h"

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    /*assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);*/
	
	return GetNextWorkRequiredwithDigiShield(pindexLast, pblock, params);
}

unsigned int GetNextWorkRequiredwithDigiShield(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
	// DigiShield Retarget Code, found in DigiByte Source

    unsigned int npowWorkLimit = UintToArith256(params.powLimit).GetCompact();
	int nHeight = pindexLast->nHeight + 1;
	int blockstogoback = 0;

	//set default to pre-v2.0 values
	int64_t retargetTimespan = params.nTargetTimespan;
	//int64_t retargetSpacing = nTargetSpacing;
	int64_t retargetInterval = params.nMinerConfirmationWindow;

	// Genesis block
	if (pindexLast == NULL)
		return npowWorkLimit;

	// Only change once per interval
	if ((pindexLast->nHeight+1) % retargetInterval != 0)
	{
		if (Params().NetworkIDString() == CBaseChainParams::TESTNET)
		{
			// Special difficulty rule for testnet:
			// If the new block's timestamp is more than 2* 10 minutes
			// then allow mining of a min-difficulty block.
			if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nTargetSpacing*2)
				return npowWorkLimit;
			else
			{
				// Return the last non-special-min-difficulty-rules-block
				const CBlockIndex* pindex = pindexLast;
				while (pindex->pprev && pindex->nHeight % retargetInterval != 0 && pindex->nBits == npowWorkLimit)
					pindex = pindex->pprev;
				return pindex->nBits;
			}
		}
		return pindexLast->nBits;
	}

	// DigiByte: This fixes an issue where a 51% attack can change difficulty at will.
	// Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
	blockstogoback = retargetInterval-1;
	if ((pindexLast->nHeight+1) != retargetInterval)
		blockstogoback = retargetInterval;

	// Go back by what we want to be 14 days worth of blocks
	const CBlockIndex* pindexFirst = pindexLast;
	for (int i = 0; pindexFirst && i < blockstogoback; i++)
		pindexFirst = pindexFirst->pprev;
	assert(pindexFirst);

	// Limit adjustment step
	int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
	LogPrintf("nActualTimespan = %d  before bounds\n", nActualTimespan);

	// thanks to RealSolid & WDC for this code
	LogPrintf("GetNextWorkRequired nActualTimespan Limiting\n");
	if (nActualTimespan < (retargetTimespan - (retargetTimespan/4)) ) nActualTimespan = (retargetTimespan - (retargetTimespan/4));
	if (nActualTimespan > (retargetTimespan + (retargetTimespan/2)) ) nActualTimespan = (retargetTimespan + (retargetTimespan/2));

	arith_uint256 bnNew;
	arith_uint256 bnBefore;
	bnNew.SetCompact(pindexLast->nBits);
	bnBefore=bnNew;
	bnNew *= nActualTimespan;
	bnNew /= retargetTimespan;

	if (bnNew > UintToArith256(params.powLimit))
		bnNew = UintToArith256(params.powLimit);

	// debug print
	LogPrintf("GetNextWorkRequired RETARGET\n");
	LogPrintf("nTargetTimespan = %d    nActualTimespan = %d\n", retargetTimespan, nActualTimespan);
	LogPrintf("Before: %08x  %s\n", pindexLast->nBits, ArithToUint256(bnBefore).ToString());
	LogPrintf("After:  %08x  %s\n", bnNew.GetCompact(), ArithToUint256(bnNew).ToString());

	return bnNew.GetCompact();
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}

bool CheckBlockProofOfWork(const CBlockHeader *pblock, const Consensus::Params& params)
{
	// There's an issue with blocks prior to the auxpow fork reporting an invalid chain ID.
	// As no version earlier than the 0.10 client a) has version 3 blocks and b) 
	//	has auxpow, anything that isn't a version 3 block can be checked normally.
	//	There's probably a more elegant way to implement this.

    bool fTestNet = GetBoolArg("-testnet", false);
    int chainID;
    if (fTestNet && pblock->nVersion >= 4)
        chainID = AUXPOW_TESTNET_CHAIN_ID;
    else
        chainID = AUXPOW_CHAIN_ID;

	if (pblock->nVersion > 2) {
       LogPrintf("nVersion : %d, ChainID : %d, %d\n",pblock->nVersion,pblock->GetChainID(),chainID);

        if (!params.fPowAllowMinDifficultyBlocks && (pblock->nVersion & BLOCK_VERSION_AUXPOW && pblock->GetChainID() != chainID))
	        return error("CheckBlockProofOfWork() : block does not have our chain ID");	

	    if (pblock->auxpow.get() != NULL)
	    {
	        if (!pblock->auxpow->Check(pblock->GetHash(), pblock->GetChainID()))
	            return error("CheckBlockProofOfWork() : AUX POW is not valid");
	        // Check proof of work matches claimed amount
	        if (!CheckProofOfWork(pblock->auxpow->GetParentBlockPoWHash(), pblock->nBits, params))
	            return error("CheckBlockProofOfWork() : AUX proof of work failed");
	    } 
	    else
	    {
	        // Check proof of work matches claimed amount
	        if (!CheckProofOfWork(pblock->GetPoWHash(), pblock->nBits, params))
	            return error("CheckBlockProofOfWork() : proof of work failed");
	    }
	}
    else
    {
        // Check proof of work matches claimed amount
        if (!CheckProofOfWork(pblock->GetPoWHash(), pblock->nBits, params))
            return error("CheckBlockProofOfWork() : proof of work failed");
    }
    return true;
}
