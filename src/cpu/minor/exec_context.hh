/*
 * Copyright (c) 2011-2014 ARM Limited
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Steve Reinhardt
 *          Dave Greene
 *          Nathan Binkert
 *          Andrew Bardsley
 */

/**
 * @file
 *
 *  ExecContext bears the exec_context interface for Minor.
 */

#ifndef __CPU_MINOR_EXEC_CONTEXT_HH__
#define __CPU_MINOR_EXEC_CONTEXT_HH__

#include "cpu/exec_context.hh"
#include "cpu/minor/execute.hh"
#include "cpu/minor/pipeline.hh"
#include "cpu/base.hh"
#include "cpu/simple_thread.hh"
#include "debug/MinorExecute.hh"
#include "base/loader/symtab.hh"
#include "debug/faultInjectionTrack.hh"
#include "debug/RegFileAccess.hh"
#include "debug/RegPointerFI.hh"
#include "debug/FUsREGfaultInjectionTrack.hh"
#include "debug/BranchsREGfaultInjectionTrack.hh"
#include "debug/CMPsREGfaultInjectionTrack.hh"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

namespace Minor
{

	/* Forward declaration of Execute */
	class Execute;

	/** ExecContext bears the exec_context interface for Minor.  This nicely
	 *  separates that interface from other classes such as Pipeline, MinorCPU
	 *  and DynMinorInst and makes it easier to see what state is accessed by it.
	 */
	class ExecContext : public ::ExecContext
	{
		public:
			MinorCPU &cpu;

			/** ThreadState object, provides all the architectural state. */
			SimpleThread &thread;

			/** The execute stage so we can peek at its contents. */
			Execute &execute;

			/** Instruction for the benefit of memory operations and for PC */
			MinorDynInstPtr inst;

			ExecContext (
					MinorCPU &cpu_,
					SimpleThread &thread_, Execute &execute_,
					MinorDynInstPtr inst_) :
				cpu(cpu_),
				thread(thread_),
				execute(execute_),
				inst(inst_)
		{
			DPRINTF(MinorExecute, "ExecContext setting PC: %s\n", inst->pc);
			pcState(inst->pc);
			setPredicate(true);
			thread.setIntReg(TheISA::ZeroReg, 0);
#if THE_ISA == ALPHA_ISA
			thread.setFloatReg(TheISA::ZeroReg, 0.0);
#endif
		}

			Fault
				readMem(Addr addr, uint8_t *data, unsigned int size,
						unsigned int flags)
				{
					execute.getLSQ().pushRequest(inst, true /* load */, data,
							size, addr, flags, NULL);
					return NoFault;
				}

			Fault
				writeMem(uint8_t *data, unsigned int size, Addr addr,
						unsigned int flags, uint64_t *res)
				{
					execute.getLSQ().pushRequest(inst, false /* store */, data,
							size, addr, flags, res);
					return NoFault;
				}
			bool inMain(const StaticInst *si)
			{
				std::string funcName;
				Addr sym_addr;
				debugSymbolTable->findNearestSymbol(inst->pc.instAddr(), funcName, sym_addr);
				if (funcName == "main" || (funcName[0] == 'F' &&  funcName[1] == 'U' && funcName[2] == 'N' && funcName[3] == 'C'))
					return true;
				return false;
			}

			IntReg
				readIntRegOperand(const StaticInst *si, int idx)
				{	//regsiter file
										
					if ((execute.FIcomparray[execute.i] == 1) && execute.faultIsInjected[execute.i] && execute.FItargetReg == si->srcRegIdx(idx) && !execute.faultGetsMasked && execute.FItargetRegClass == Execute::regClass::INTEGER)
					{
						std::string funcName;
						Addr sym_addr;
						debugSymbolTable->findNearestSymbol(inst->pc.instAddr(), funcName, sym_addr);
						DPRINTF(faultInjectionTrack, "In Function: %s instruction  %s is reading faulty register %s\n which the faulty value is %s\n", funcName, inst->staticInst->disassemble(0), si->srcRegIdx(idx), thread.readIntReg(si->srcRegIdx(idx) ));
					}
					
					// registers pointer in pipeline
					//else if ((execute.FIcomparray[execute.i] == 3) && (!execute.faultIsInjected[execute.i]) && execute.FItarget == execute.headOfInFlightInst &&  execute.FItargetReg == si->srcRegIdx(idx) && execute.pipelineRegisters)
					else if ((execute.FIcomparray[execute.i] == 3) && (!execute.faultIsInjected[execute.i]) && (execute.FItarget == curTick()) && execute.pipelineRegisters)
					{
						float probability = ( (float)execute.inputBuffer.getSizeBuffer()/(float)7 ); //likeliness of fault happening on valid inst
						int randvar=rand()%100; //Generates a random number from 0-100 over uniform dist. 
						float result=(float)randvar/(float)100;
						if (result > probability) {
						DPRINTF(RegPointerFI, ANSI_COLOR_YELLOW "####In Pipeline, Fault Prob. on valid inst was %d/%d. Skipping FI####" ANSI_COLOR_RESET "\n", execute.inputBuffer.getSizeBuffer(),7);
						srand (curTick()+time(0));
						execute.faultIsInjected[execute.i]=true;
						execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
						return thread.readIntReg(si->srcRegIdx(idx)); //Returning true value
					        }	
						else
						{	
						srand (time(0));
						int faultyIDX = rand()%(34); 
						if(faultyIDX == 33) faultyIDX = NUM_INTREGS;
						DPRINTF(RegPointerFI, ANSI_COLOR_BLUE "----PIPELINE FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
						//srand (time(0));
						//randBit = rand()%62;
						//temp = pow (2, randBit);
						execute.faultIsInjected[execute.i]=true;
						execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }

				DPRINTF(RegPointerFI, "%s, points to I: %s\nBecause of faults in pipeline registers now it points to %s\n", inst->staticInst->disassemble(0), static_cast<unsigned int>(si->srcRegIdx(idx)), static_cast<unsigned int>(faultyIDX));
						return thread.readIntReg(faultyIDX);

						}
					}
					// FUs fault injection for ADDress calculation of memory operands
					//else if ((execute.FIcomparray[execute.i] == 4) && (!execute.faultIsInjected[execute.i]) && ( execute.FItarget == execute.headOfInFlightInst || execute.FItarget == inst->id.execSeqNum)   && execute.FUsFI )
					else if ((execute.FIcomparray[execute.i] == 4) && (!execute.faultIsInjected[execute.i]) && ( execute.FItarget == curTick())   && execute.FUsFI )
					{
						float probability = ( (float)execute.inputBuffer.getSizeBuffer()/(float)7 ); //likeliness of fault happening on valid inst
						int randvar=rand()%100; //Generates a random number from 0-100 over uniform dist. 
						float result=(float)randvar/(float)100;
						if (result > probability) {
						DPRINTF(RegPointerFI, ANSI_COLOR_YELLOW "####In FU, Fault Prob. on valid inst was %d/%d. Skipping FI####" ANSI_COLOR_RESET "\n", execute.inputBuffer.getSizeBuffer(),7);
						srand (curTick()+time(0));
						execute.faultIsInjected[execute.i]=true;
						execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
						return thread.readIntReg(si->srcRegIdx(idx)); //Returning true value
					        }	
						else
						{
						srand (time(0));
						int faultyBIT = rand()%(32); 
						int temp = pow (2, faultyBIT);
						int faultyval = thread.readIntReg(si->srcRegIdx(idx)) xor temp; 
						if (faultyval < 0) faultyval= -faultyval;
			DPRINTF(FUsREGfaultInjectionTrack, ANSI_COLOR_BLUE "----FU FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
						execute.faultIsInjected[execute.i]=true;
						execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
						DPRINTF(FUsREGfaultInjectionTrack, "%s: " ANSI_COLOR_GREEN "TRUE FU's VALUE" ANSI_COLOR_RESET " was: %s due to the faults in FU's registers the " ANSI_COLOR_RED "FAULTY FU's VALUE " ANSI_COLOR_RESET " is %s\n", inst->staticInst->disassemble(0), thread.readIntReg(si->srcRegIdx(idx)), faultyval);
						return faultyval;
						}
					}
					// fault injection for branchs registers
					else if (!execute.faultIsInjected[execute.i] && (execute.FItarget == execute.headOfInFlightInst ) /*&&  execute.FItargetReg == si->srcRegIdx(idx)*/ && execute.BranchsFI )
					{
						srand (time(0));
						int faultyBIT = rand()%(32); 
						int temp = pow (2, faultyBIT);
						execute.faultIsInjected[execute.i]=true;
						int faultyval = thread.readIntReg(si->srcRegIdx(idx)) xor temp; 

						DPRINTF(BranchsREGfaultInjectionTrack, "%s: true Branch register val was: %s\nBecause of fault now the value is %s\n", inst->staticInst->disassemble(0), thread.readIntReg(si->srcRegIdx(idx)), faultyval);
								thread.setIntReg(si->srcRegIdx(idx), faultyval);
					}
				else if (!execute.faultIsInjected[execute.i] && (execute.FItarget == execute.headOfInFlightInst ) /*&&  execute.FItargetReg == si->srcRegIdx(idx)*/ && execute.CMPsFI && !si->isLoad() && !si->isStore() )
					{
						srand (time(0));
						int faultyBIT = rand()%(32); 
						int temp = pow (2, faultyBIT);
						execute.faultIsInjected[execute.i]=true;
						int faultyval = thread.readIntReg(si->srcRegIdx(idx)) xor temp; 

						DPRINTF(CMPsREGfaultInjectionTrack, "%s: true CMP register val was: %s\nBecause of fault now the value is %s\n", inst->staticInst->disassemble(0), thread.readIntReg(si->srcRegIdx(idx)), faultyval);
								return faultyval;
					}
				return thread.readIntReg(si->srcRegIdx(idx));
				}
								

								TheISA::FloatReg
								readFloatRegOperand(const StaticInst *si, int idx)
								{
								int reg_idx = si->srcRegIdx(idx) - TheISA::FP_Reg_Base;
								//std::cout << "Inst: " << inst->staticInst->disassemble(0) << " reg_idx:" << reg_idx << "\n";
								if ((execute.FIcomparray[execute.i] == 1) && execute.faultIsInjected[execute.i] && execute.FItargetReg == reg_idx && !execute.faultGetsMasked && execute.FItargetRegClass == Execute::regClass::FLOAT)
								{
								std::string funcName;
								Addr sym_addr;
								debugSymbolTable->findNearestSymbol(inst->pc.instAddr(), funcName, sym_addr);
								DPRINTF(faultInjectionTrack, "In Function: %s instruction  %s is reading faulty register %s\n which the faulty value is %s\n", funcName, inst->staticInst->disassemble(0), reg_idx, thread.readFloatReg(reg_idx));
								}
								// registers pointer in pipeline
								//else if ((execute.FIcomparray[execute.i] == 3) && (!execute.faultIsInjected[execute.i]) && execute.FItarget == execute.headOfInFlightInst &&  execute.FItargetReg == reg_idx && execute.pipelineRegisters)
								else if ((execute.FIcomparray[execute.i] == 3) && (!execute.faultIsInjected[execute.i]) && (execute.FItarget == curTick()) && execute.pipelineRegisters)
								{
								float probability = ( (float)execute.inputBuffer.getSizeBuffer()/(float)7 ); //likeliness of fault happening on valid inst
								int randvar=rand()%100; //Generates a random number from 0-100 over uniform dist. 
								float result=(float)randvar/(float)100;
								if (result > probability) {
								DPRINTF(RegPointerFI, ANSI_COLOR_YELLOW "####In Pipeline, Fault Prob. on valid inst was %d/%d. Skipping FI####" ANSI_COLOR_RESET "\n", execute.inputBuffer.getSizeBuffer(),7);
								srand (curTick()+time(0));
								execute.faultIsInjected[execute.i]=true;
								execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
								return thread.readFloatReg(reg_idx);
								}
								else
								{	
									srand (time(0));
									int faultyIDX = rand()%(30); 
				DPRINTF(RegPointerFI, ANSI_COLOR_BLUE "----PIPELINE FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
									//srand (time(0));
									//randBit = rand()%62;
									//temp = pow (2, randBit);
									execute.faultIsInjected[execute.i]=true;
									execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
				DPRINTF(RegPointerFI, "%s: Idx(%s), points to F: %s\nBecause of faults in pipeline registers now it points to %s\n", inst->staticInst->disassemble(0), reg_idx, static_cast<unsigned int>(reg_idx), static_cast<unsigned int>(faultyIDX));
									return thread.readFloatReg(faultyIDX);

								}
								}
					//else if ((execute.FIcomparray[execute.i] == 4) && (!execute.faultIsInjected[execute.i]) && (execute.FItarget == execute.headOfInFlightInst || execute.FItarget == inst->id.execSeqNum) /*&&  execute.FItargetReg == si->srcRegIdx(idx)*/ && execute.FUsFI )
					else if ((execute.FIcomparray[execute.i] == 4) && (!execute.faultIsInjected[execute.i]) && (execute.FItarget == curTick() ) && execute.FUsFI )
					{
								float probability = ( (float)execute.inputBuffer.getSizeBuffer()/(float)7 ); //likeliness of fault happening on valid inst
								int randvar=rand()%100; //Generates a random number from 0-100 over uniform dist. 
								float result=(float)randvar/(float)100;
								if (result > probability) {
								DPRINTF(RegPointerFI, ANSI_COLOR_YELLOW "####In FU, Fault Prob. on valid inst was %d/%d. Skipping FI####" ANSI_COLOR_RESET "\n", execute.inputBuffer.getSizeBuffer(),7);
								srand (curTick()+time(0));
								execute.faultIsInjected[execute.i]=true;
								execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
								return thread.readFloatReg(reg_idx);
								}
								else
								{	
						srand (time(0));
						int faultyBIT = rand()%(32); 
						int temp = pow (2, faultyBIT);
			DPRINTF(FUsREGfaultInjectionTrack, ANSI_COLOR_BLUE "----FU FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
						execute.faultIsInjected[execute.i]=true;
						execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
						int faultyval = int(thread.readFloatReg(reg_idx)) xor temp; 
if (faultyval < 0) faultyval= -faultyval;
						DPRINTF(FUsREGfaultInjectionTrack, "%s: " ANSI_COLOR_GREEN "TRUE FU's VALUE" ANSI_COLOR_RESET " was: %s\nDue to the faults in FU's registers the " ANSI_COLOR_RED "FAULTY FU's VALUE " ANSI_COLOR_RESET " is %s\n", inst->staticInst->disassemble(0), thread.readFloatReg(reg_idx), faultyval);
						return faultyval;
								}
					}

								return thread.readFloatReg(reg_idx);
								}

						TheISA::FloatRegBits
							readFloatRegOperandBits(const StaticInst *si, int idx)
							{
								int reg_idx = si->srcRegIdx(idx) - TheISA::FP_Reg_Base;

								if ((execute.FIcomparray[execute.i] == 1) && execute.faultIsInjected[execute.i] && execute.FItargetReg == reg_idx && !execute.faultGetsMasked && execute.FItargetRegClass == Execute::regClass::FLOAT)
								{
									std::string funcName;
									Addr sym_addr;
									debugSymbolTable->findNearestSymbol(inst->pc.instAddr(), funcName, sym_addr);
									DPRINTF(faultInjectionTrack, "In Function: %s instruction  %s is reading faulty register %s\n which the faulty value is %s\n", funcName, inst->staticInst->disassemble(0), reg_idx, thread.readFloatRegBits(reg_idx));
								}
								// registers pointer in pipeline
								//if ((execute.FIcomparray[execute.i] == 3) && (!execute.faultIsInjected[execute.i]) && execute.FItarget == execute.headOfInFlightInst &&  execute.FItargetReg == reg_idx && execute.pipelineRegisters)
								if ((execute.FIcomparray[execute.i] == 3) && (!execute.faultIsInjected[execute.i]) && (execute.FItarget == curTick()) &&  execute.pipelineRegisters)
								{
								float probability = ( (float)execute.inputBuffer.getSizeBuffer()/(float)7 ); //likeliness of fault happening on valid inst
								int randvar=rand()%100; //Generates a random number from 0-100 over uniform dist. 
								float result=(float)randvar/(float)100;
								if (result > probability) {
								DPRINTF(RegPointerFI, ANSI_COLOR_YELLOW "####In Pipeline, Fault Prob. on valid inst was %d/%d. Skipping FI####" ANSI_COLOR_RESET "\n", execute.inputBuffer.getSizeBuffer(),7);
								srand (curTick()+time(0));
								execute.faultIsInjected[execute.i]=true;
								execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
								return thread.readFloatRegBits(reg_idx);
								}
								else
								{	srand (time(0));
									int faultyIDX = rand()%(30); 
									//srand (time(0));
									//randBit = rand()%62;
									//temp = pow (2, randBit);
									execute.faultIsInjected[execute.i]=true;
									execute.i = execute.i + 1;	
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
				DPRINTF(RegPointerFI, ANSI_COLOR_BLUE "----PIPELINE FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
				DPRINTF(RegPointerFI, "%s: Idx(%s), points to F: %s\nBecause of faults in pipeline registers now it points to %s\n", inst->staticInst->disassemble(0), reg_idx, static_cast<unsigned int>(reg_idx), static_cast<unsigned int>(faultyIDX));
									return thread.readFloatRegBits(faultyIDX);

								}
								}
					//else if ((execute.FIcomparray[execute.i] == 4) && (!execute.faultIsInjected[execute.i]) && (execute.FItarget == execute.headOfInFlightInst || execute.FItarget == inst->id.execSeqNum) /*&&  execute.FItargetReg == si->srcRegIdx(idx)*/ && execute.FUsFI )
					else if ((execute.FIcomparray[execute.i] == 4) && (!execute.faultIsInjected[execute.i]) && (execute.FItarget == curTick()) && execute.FUsFI )
					{
								float probability = ( (float)execute.inputBuffer.getSizeBuffer()/(float)7 ); //likeliness of fault happening on valid inst
								int randvar=rand()%100; //Generates a random number from 0-100 over uniform dist. 
								float result=(float)randvar/(float)100;
								if (result > probability) {
								DPRINTF(RegPointerFI, ANSI_COLOR_YELLOW "####In FU, Fault Prob. on valid inst was %d/%d. Skipping FI####" ANSI_COLOR_RESET "\n", execute.inputBuffer.getSizeBuffer(),7);
								srand (curTick()+time(0));
								execute.faultIsInjected[execute.i]=true;
								execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
								return thread.readFloatRegBits(reg_idx);
								}
								else
								{
								srand (time(0));
								int faultyBIT = rand()%(32); 
								int temp = pow (2, faultyBIT);
								DPRINTF(FUsREGfaultInjectionTrack, ANSI_COLOR_BLUE "----FU FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
								execute.faultIsInjected[execute.i]=true;
								execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
						int faultyval = int(thread.readFloatRegBits(reg_idx)) xor temp; 
if (faultyval < 0) faultyval= -faultyval;
						DPRINTF(FUsREGfaultInjectionTrack, "%s: " ANSI_COLOR_GREEN "TRUE FU's VALUE" ANSI_COLOR_RESET " was: %s\nDue to the faults in FU's registers the " ANSI_COLOR_RED "FAULTY FU's VALUE " ANSI_COLOR_RESET " is %s\n", inst->staticInst->disassemble(0), thread.readFloatRegBits(reg_idx), faultyval);
return faultyval;
								}
					 }
								return thread.readFloatRegBits(reg_idx);
							}

						void
							setIntRegOperand(const StaticInst *si, int idx, IntReg val)
							{

								if ((execute.FIcomparray[execute.i] == 1) && execute.faultIsInjected[execute.i] && execute.FItargetReg == si->destRegIdx(idx) && !execute.faultGetsMasked && execute.FItargetRegClass == Execute::regClass::INTEGER)
								{
									std::string funcName;
									Addr sym_addr;
									debugSymbolTable->findNearestSymbol(inst->pc.instAddr(), funcName, sym_addr);
									DPRINTF(faultInjectionTrack, "In Function: %s instruction  %s is overwritten the faulty register %s\n, which the faulty value was %s, with %s!\n", funcName, inst->staticInst->disassemble(0), si->destRegIdx(idx), thread.readIntReg(si->destRegIdx(idx)), val);
									execute.faultGetsMasked=true;

								}
								// registers pointer in pipeline
								//else if ((execute.FIcomparray[execute.i] == 3) && (!execute.faultIsInjected[execute.i]) && execute.FItarget == execute.headOfInFlightInst &&  execute.FItargetReg == si->destRegIdx(idx) && execute.pipelineRegisters)
					else if ((execute.FIcomparray[execute.i] == 3) && (!execute.faultIsInjected[execute.i]) && (execute.FItarget == curTick()) &&  execute.pipelineRegisters)
					{
						float probability = ( (float)execute.inputBuffer.getSizeBuffer()/(float)7 ); //likeliness of fault happening on valid inst
						int randvar=rand()%100; //Generates a random number from 0-100 over uniform dist. 
						float result=(float)randvar/(float)100;
						if (result > probability) {
						DPRINTF(RegPointerFI, ANSI_COLOR_YELLOW "####In Pipeline, Fault Prob. on valid inst was %d/%d. Skipping FI####" ANSI_COLOR_RESET "\n", execute.inputBuffer.getSizeBuffer(),7);
						srand (curTick()+time(0));
						execute.faultIsInjected[execute.i]=true;
						execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
								thread.setIntReg(si->destRegIdx(idx), val);
						}
						else
						{
									srand (time(0));
									int faultyIDX = rand()%(34); 
									if(faultyIDX == 33) faultyIDX = NUM_INTREGS;
									//randBit = rand()%62;
									//temp = pow (2, randBit);
				DPRINTF(RegPointerFI, ANSI_COLOR_BLUE "----PIPELINE FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
									execute.faultIsInjected[execute.i]=true;
									execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
									DPRINTF(RegPointerFI, "%s: Idx(%s), points to I: %s\nBecause of faults in pipeline registers now it points to %s\n", inst->staticInst->disassemble(0), idx, static_cast<unsigned int>(si->destRegIdx(idx)), static_cast<unsigned int>(faultyIDX));
									thread.setIntReg(faultyIDX, val);
									return;

						}
					}
								//else if ((execute.FIcomparray[execute.i] == 4) && (!execute.faultIsInjected[execute.i]) && execute.FItarget == execute.headOfInFlightInst &&  execute.FItargetReg == si->destRegIdx(idx) && execute.FUsFI && false)
								else if ((execute.FIcomparray[execute.i] == 4) && (!execute.faultIsInjected[execute.i]) && execute.FItarget == curTick() && execute.FUsFI && false)
								{
									srand (time(0));
									int faultyBIT = rand()%(32); 
									int temp = pow (2, faultyBIT);
                           DPRINTF(FUsREGfaultInjectionTrack, ANSI_COLOR_BLUE "----FU FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
									execute.faultIsInjected[execute.i]=true;
									execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
									int faultyval = val xor temp; 
if (faultyval < 0) faultyval= -faultyval;
DPRINTF(FUsREGfaultInjectionTrack, "%s: " ANSI_COLOR_GREEN "TRUE FU's VALUE" ANSI_COLOR_RESET " was: %s\nDue to the faults in FU's registers the " ANSI_COLOR_RED "FAULTY FU's VALUE " ANSI_COLOR_RESET " is %s\n", inst->staticInst->disassemble(0), val, faultyval);
									thread.setIntReg(si->destRegIdx(idx), faultyval);
									return;

								}
								thread.setIntReg(si->destRegIdx(idx), val);
							}

						void
							setFloatRegOperand(const StaticInst *si, int idx,
									TheISA::FloatReg val)
							{
								int reg_idx = si->destRegIdx(idx) - TheISA::FP_Reg_Base;


								if ((execute.FIcomparray[execute.i] == 1) && execute.faultIsInjected[execute.i] && execute.FItargetReg == reg_idx && !execute.faultGetsMasked && execute.FItargetRegClass == Execute::regClass::FLOAT) 
								{
									std::string funcName;
									Addr sym_addr;
									debugSymbolTable->findNearestSymbol(inst->pc.instAddr(), funcName, sym_addr);
									DPRINTF(faultInjectionTrack, "In Function: %s instruction  %s is overwritten the faulty register %s\n which the faulty value was %s, with %s!\n", funcName, inst->staticInst->disassemble(0), reg_idx, thread.readFloatReg(reg_idx), val);
									execute.faultGetsMasked=true;

								}
								// registers pointer in pipeline
								//else if ((execute.FIcomparray[execute.i] == 3) && (!execute.faultIsInjected[execute.i]) && execute.FItarget == execute.headOfInFlightInst &&  execute.FItargetReg == reg_idx && execute.pipelineRegisters)
								else if ((execute.FIcomparray[execute.i] == 3) && (!execute.faultIsInjected[execute.i]) && (execute.FItarget == curTick()) && execute.pipelineRegisters)
								{
								float probability = ( (float)execute.inputBuffer.getSizeBuffer()/(float)7 ); //likeliness of fault happening on valid inst
								int randvar=rand()%100; //Generates a random number from 0-100 over uniform dist. 
								float result=(float)randvar/(float)100;
								if (result > probability) {
								DPRINTF(RegPointerFI, ANSI_COLOR_YELLOW "####In Pipeline, Fault Prob. on valid inst was %d/%d. Skipping FI####" ANSI_COLOR_RESET "\n", execute.inputBuffer.getSizeBuffer(),7);
								srand (curTick()+time(0));
								execute.faultIsInjected[execute.i]=true;
								execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
								thread.setFloatReg(reg_idx, val);
								}
								else
								{
									srand (time(0));
									int faultyIDX = rand()%(NUM_INTREGS); 
									//srand (time(0));
									//randBit = rand()%62;
									//temp = pow (2, randBit);
				DPRINTF(RegPointerFI, ANSI_COLOR_BLUE "----PIPELINE FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
									execute.faultIsInjected[execute.i]=true;
									execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
									DPRINTF(RegPointerFI, "%s: Idx(%s), points to F: %s\nBecause of faults in pipeline registers now it points to %s\n", inst->staticInst->disassemble(0), idx, static_cast<unsigned int>(reg_idx), static_cast<unsigned int>(faultyIDX));
									thread.setFloatReg(faultyIDX, val);
									return;

								}
								}
								//else if ((execute.FIcomparray[execute.i] == 4) && (!execute.faultIsInjected[execute.i]) && execute.FItarget == execute.headOfInFlightInst &&  execute.FItargetReg == reg_idx && execute.FUsFI && false)
								else if ((execute.FIcomparray[execute.i] == 4) && (!execute.faultIsInjected[execute.i]) && execute.FItarget == curTick() && execute.FUsFI && false)
								{
									srand (time(0));
									int faultyBIT = rand()%(32); 
									TheISA::FloatReg temp = pow (2, faultyBIT);
			DPRINTF(FUsREGfaultInjectionTrack, ANSI_COLOR_BLUE "----FU FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
									execute.faultIsInjected[execute.i]=true;
									execute.i = execute.i + 1;
									TheISA::FloatReg faultyval = (long)val xor (long)temp; 

									DPRINTF(FUsREGfaultInjectionTrack, "%s: " ANSI_COLOR_GREEN "TRUE FU's VALUE" ANSI_COLOR_RESET " was: %s\nDue to the faults in FU's registers the " ANSI_COLOR_RED "FAULTY FU's VALUE " ANSI_COLOR_RESET " is %s\n", inst->staticInst->disassemble(0), val, faultyval);
									thread.setFloatReg(reg_idx, faultyval);
									return;

								}
								thread.setFloatReg(reg_idx, val);
							}

						void
							setFloatRegOperandBits(const StaticInst *si, int idx,
									TheISA::FloatRegBits val)
							{

								int reg_idx = si->destRegIdx(idx) - TheISA::FP_Reg_Base;
								//std::cout << "Inst, " << inst->staticInst->disassemble(0) << " idx, " << idx<< " reg_idx, " << reg_idx << "\n";
								if ((execute.FIcomparray[execute.i] == 1) &&  execute.faultIsInjected[execute.i] && execute.FItargetReg == reg_idx && !execute.faultGetsMasked && execute.FItargetRegClass == Execute::regClass::FLOAT) 
								{
									std::string funcName;
									Addr sym_addr;
									debugSymbolTable->findNearestSymbol(inst->pc.instAddr(), funcName, sym_addr);
									DPRINTF(faultInjectionTrack, "In Function: %s instruction  %s is overwritten the faulty register %s\n which the faulty value was %s, with %s!\n", funcName, inst->staticInst->disassemble(0), reg_idx, thread.readFloatRegBits(reg_idx), val);
									execute.faultGetsMasked=true;
									
								}
								// registers pointer in pipeline
								//else if ((execute.FIcomparray[execute.i] == 3) && !execute.faultIsInjected[execute.i] && execute.FItarget == execute.headOfInFlightInst &&  execute.FItargetReg == reg_idx && execute.pipelineRegisters)
								else if ((execute.FIcomparray[execute.i] == 3) && !execute.faultIsInjected[execute.i] && (execute.FItarget == curTick()) &&  execute.pipelineRegisters)
								{
								float probability = ( (float)execute.inputBuffer.getSizeBuffer()/(float)7 ); //likeliness of fault happening on valid inst
								int randvar=rand()%100; //Generates a random number from 0-100 over uniform dist. 
								float result=(float)randvar/(float)100;
								if (result > probability) {
								DPRINTF(RegPointerFI, ANSI_COLOR_YELLOW "####In Pipeline, Fault Prob. on valid inst was %d/%d. Skipping FI####" ANSI_COLOR_RESET "\n", execute.inputBuffer.getSizeBuffer(),7);
								srand (curTick()+time(0));
								execute.faultIsInjected[execute.i]=true;
								execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
								thread.setFloatRegBits(reg_idx, val);
								}
								else
								{
									srand (time(0));
									int faultyIDX = rand()%(NUM_INTREGS); 
									//srand (time(0));
									//randBit = rand()%62;
									//temp = pow (2, randBit);
				DPRINTF(RegPointerFI, ANSI_COLOR_BLUE "----PIPELINE FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
									execute.faultIsInjected[execute.i]=true;
									execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
									DPRINTF(RegPointerFI, "%s: Idx(%s), points to F: %s\nBecause of faults in pipeline registers now it points to %s\n", inst->staticInst->disassemble(0), idx, static_cast<unsigned int>(reg_idx), static_cast<unsigned int>(faultyIDX));
									thread.setFloatRegBits(faultyIDX, val);
									return;

								}	
								}
								//FUs fault injection
								else if ((execute.FIcomparray[execute.i] == 4) &&  !execute.faultIsInjected[execute.i] && execute.FItarget == execute.headOfInFlightInst &&  execute.FItargetReg == reg_idx && execute.FUsFI && false)
								{
									srand (time(0));
									int faultyBIT = rand()%(32); 
									TheISA::FloatReg temp = pow (2, faultyBIT);
			DPRINTF(FUsREGfaultInjectionTrack, ANSI_COLOR_BLUE "----FU FI--FAULT ID=%d----@ clk tick=%s with Seq Num=%s" ANSI_COLOR_RESET "\n", execute.i,curTick(),inst->id.execSeqNum);
									execute.faultIsInjected[execute.i]=true;
									execute.i = execute.i + 1;
if (execute.FIcomparray[execute.i] == 1) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=100; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 2) { execute.pipelineRegisters=false; execute.FUsFI=false; execute.LSQFI=true; execute.FItargetReg=225; execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 3) { execute.pipelineRegisters=true; execute.FUsFI=false; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
if (execute.FIcomparray[execute.i] == 4) { execute.pipelineRegisters=false; execute.FUsFI=true; execute.LSQFI=false; execute.FItargetReg=execute.FIRegArr[execute.i];execute.FItarget=execute.FItarArr[execute.i]; }
 									TheISA::FloatReg faultyval = (long)val xor (long)temp; 
if (faultyval < 0) faultyval= -faultyval;
									DPRINTF(FUsREGfaultInjectionTrack, "%s: " ANSI_COLOR_GREEN "TRUE FU's VALUE" ANSI_COLOR_RESET " was: %s\nDue to the faults in FU's registers the " ANSI_COLOR_RED "FAULTY FU's VALUE " ANSI_COLOR_RESET " is %s\n", inst->staticInst->disassemble(0), val, faultyval);
									thread.setFloatRegBits(reg_idx, faultyval);
									return;

								}
								thread.setFloatRegBits(reg_idx, val);
							}

						bool
							readPredicate()
							{
								return thread.readPredicate();
							}

						void
							setPredicate(bool val)
							{
								thread.setPredicate(val);
							}

						TheISA::PCState
							pcState() const
							{
								return thread.pcState();
							}

						void
							pcState(const TheISA::PCState &val)
							{
								thread.pcState(val);
							}

						TheISA::MiscReg
							readMiscRegNoEffect(int misc_reg) const
							{
								return thread.readMiscRegNoEffect(misc_reg);
							}

						TheISA::MiscReg
							readMiscReg(int misc_reg)
							{
								return thread.readMiscReg(misc_reg);
							}

						void
							setMiscReg(int misc_reg, const TheISA::MiscReg &val)
							{
								thread.setMiscReg(misc_reg, val);
							}

						TheISA::MiscReg
							readMiscRegOperand(const StaticInst *si, int idx)
							{
								int reg_idx = si->srcRegIdx(idx) - TheISA::Misc_Reg_Base;
								return thread.readMiscReg(reg_idx);
							}

						void
							setMiscRegOperand(const StaticInst *si, int idx,
									const TheISA::MiscReg &val)
							{
								int reg_idx = si->destRegIdx(idx) - TheISA::Misc_Reg_Base;
								return thread.setMiscReg(reg_idx, val);
							}

						Fault
							hwrei()
							{
#if THE_ISA == ALPHA_ISA
								return thread.hwrei();
#else
								return NoFault;
#endif
							}

						bool
							simPalCheck(int palFunc)
							{
#if THE_ISA == ALPHA_ISA
								return thread.simPalCheck(palFunc);
#else
								return false;
#endif
							}

						void
							syscall(int64_t callnum)
							{
								if (FullSystem)
									panic("Syscall emulation isn't available in FS mode.\n");

								thread.syscall(callnum);
							}

						ThreadContext *tcBase() { return thread.getTC(); }

						/* @todo, should make stCondFailures persistent somewhere */
						unsigned int readStCondFailures() const { return 0; }
						void setStCondFailures(unsigned int st_cond_failures) {}

						int contextId() { return thread.contextId(); }
						/* ISA-specific (or at least currently ISA singleton) functions */

						/* X86: TLB twiddling */
						void
							demapPage(Addr vaddr, uint64_t asn)
							{
								thread.getITBPtr()->demapPage(vaddr, asn);
								thread.getDTBPtr()->demapPage(vaddr, asn);
							}

						TheISA::CCReg
							readCCRegOperand(const StaticInst *si, int idx)
							{
								int reg_idx = si->srcRegIdx(idx) - TheISA::CC_Reg_Base;

					if (!execute.faultIsInjected[execute.i] && (execute.FItarget == execute.headOfInFlightInst ) && execute.BranchsFI && execute.FItargetReg ==  reg_idx)
					{
						//srand (time(0));
						//int faultyBIT = rand()%(32); 
						//int temp = pow (2, faultyBIT);
						execute.faultIsInjected[execute.i]=true;
						int faultyval = !thread.readCCReg(reg_idx); 

						DPRINTF(BranchsREGfaultInjectionTrack, "%s: true CC Branch register val was: %s\nBecause of fault now the value is %s\n", inst->staticInst->disassemble(0), thread.readCCReg(reg_idx), faultyval);
								thread.setCCReg(reg_idx, faultyval);
					}

								return thread.readCCReg(reg_idx);
							}

						void
							setCCRegOperand(const StaticInst *si, int idx, TheISA::CCReg val)
							{
								int reg_idx = si->destRegIdx(idx) - TheISA::CC_Reg_Base;
								thread.setCCReg(reg_idx, val);
							}

						void
							demapInstPage(Addr vaddr, uint64_t asn)
							{
								thread.getITBPtr()->demapPage(vaddr, asn);
							}

						void
							demapDataPage(Addr vaddr, uint64_t asn)
							{
								thread.getDTBPtr()->demapPage(vaddr, asn);
							}

						/* ALPHA/POWER: Effective address storage */
						void setEA(Addr ea)
						{
							inst->ea = ea;
						}

						BaseCPU *getCpuPtr() { return &cpu; }

						/* POWER: Effective address storage */
						Addr getEA() const
						{
							return inst->ea;
						}

						/* MIPS: other thread register reading/writing */
						uint64_t
							readRegOtherThread(int idx, ThreadID tid = InvalidThreadID)
							{
								SimpleThread *other_thread = (tid == InvalidThreadID
										? &thread : cpu.threads[tid]);

								if (idx < TheISA::FP_Reg_Base) { /* Integer */
									return other_thread->readIntReg(idx);
								} else if (idx < TheISA::Misc_Reg_Base) { /* Float */
									return other_thread->readFloatRegBits(idx
											- TheISA::FP_Reg_Base);
								} else { /* Misc */
									return other_thread->readMiscReg(idx
											- TheISA::Misc_Reg_Base);
								}
							}

						void
							setRegOtherThread(int idx, const TheISA::MiscReg &val,
									ThreadID tid = InvalidThreadID)
							{
								SimpleThread *other_thread = (tid == InvalidThreadID
										? &thread : cpu.threads[tid]);

								if (idx < TheISA::FP_Reg_Base) { /* Integer */
									return other_thread->setIntReg(idx, val);
								} else if (idx < TheISA::Misc_Reg_Base) { /* Float */
									return other_thread->setFloatRegBits(idx
											- TheISA::FP_Reg_Base, val);
								} else { /* Misc */
									return other_thread->setMiscReg(idx
											- TheISA::Misc_Reg_Base, val);
								}
							}

	public:
						// monitor/mwait funtions
						void armMonitor(Addr address) { getCpuPtr()->armMonitor(address); }
						bool mwait(PacketPtr pkt) { return getCpuPtr()->mwait(pkt); }
						void mwaitAtomic(ThreadContext *tc)
						{ return getCpuPtr()->mwaitAtomic(tc, thread.dtb); }
						AddressMonitor *getAddrMonitor()
						{ return getCpuPtr()->getCpuAddrMonitor(); }
};

}

#endif /* __CPU_MINOR_EXEC_CONTEXT_HH__ */
