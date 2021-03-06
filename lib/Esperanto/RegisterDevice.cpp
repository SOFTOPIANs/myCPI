/***
 * LoadCompareNamer.cpp
 *
 * Module pass to load the metadata from the file "metadata.profile", 
 * which is printed by "corelab/Metadata/Namer.h".
 * 
 * */

#define DEBUG_TYPE "device_linker"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/CallSite.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/Passes.h"

#include "corelab/Esperanto/RegisterDevice.h"
#include "corelab/Utilities/GlobalCtors.h"

#include "corelab/Utilities/InstInsertPt.h"
#include "corelab/Utilities/Casting.h"
//#include "corelab/Esperanto/EspInit.h"

#include <iostream>
#include <vector>
#include <list>
#include <cstdlib>
#include <cstdio>
#include <stdio.h>
#include <string.h>
#include <cxxabi.h>
using namespace corelab;

namespace corelab {

	char DeviceLinker::ID = 0;
	static RegisterPass<DeviceLinker> X("deviceLinker", "create main fcn if there is no main fcn", false, false);

	static cl::opt<string> DeviceName("device_register", cl::desc("device name for functions"),cl::value_desc("device name"));

	void DeviceLinker::getAnalysisUsage(AnalysisUsage& AU) const {
		AU.setPreservesAll();
	}

	bool DeviceLinker::runOnModule(Module& M)
	{
		//LLVMContext &Context = getGlobalContext();
		setFunctions(M);
		insertRegisterDevice(M);
		return false;
	}

	void DeviceLinker::setFunctions(Module& M){
		RegisterDevice = M.getOrInsertFunction("registerDevice",
				Type::getVoidTy(M.getContext()),
				Type::getInt8PtrTy(M.getContext()),
				(Type*)0);

    UvaSync = M.getOrInsertFunction("uva_sync",
        Type::getVoidTy(M.getContext()),
        (Type*)0);
	}

	void DeviceLinker::insertRegisterDevice(Module& M){
		typedef Module::iterator FF;
		typedef Function::iterator BB;
		typedef BasicBlock::iterator II;

		//const DataLayout &Layout = M.getDataLayout();
    //LLVMContext &Ctx = M.getContext();

    std::vector<Value*> actuals(1);

    // no need to generate a name of the constructor
		//StringRef ctorName = createConstructorName();
		//DEBUG(errs() << "ctorName is " << ctorName.data() << "\n");

		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			//if (F->isDeclaration()) continue;
			for(BB BI = F->begin(),BE = F->end();BI != BE; ++BI){
        BasicBlock* B = (BasicBlock*) &*BI;
        for(II Ii = B->begin(),IE = B->end();Ii != IE; ++Ii){
          Instruction* inst = (Instruction*)&*Ii;
          MDNode* MD = inst->getMetadata("esperanto.constructor");
				  if (MD == NULL) continue;
          actuals[0] = inst;

          InstInsertPt out;
          // Invoke function jumps to a invoke destination 
          if(InvokeInst* Invoke = dyn_cast<InvokeInst>(inst))
            out = InstInsertPt::Beginning(Invoke->getNormalDest());
          else
            out = InstInsertPt::After(inst);

          out << CallInst::Create(RegisterDevice, actuals);
          //actuals.resize(0);
          //out << CallInst::Create(UvaSync, actuals);
          //actuals.resize(1);
          
          /*
          if(CallInst* ci = dyn_cast<CallInst>(inst)){
            Value* ctor_v = ci->getCalledValue();
            Function* ctor = ci->getCalledFunction();
            if(ctor != NULL){

              int status = 0;
              //char* ctor_demangled = abi::__cxa_demangle(ctor->getName().data(),0,0,&status);
              //if(ctor_demangled == NULL) continue;
              //DEBUG(errs() << "\n\nctor demangled " << ctor_demangled << "\n\n");

              //if(strcmp(ctorName.data(),ctor->getName().data()) == 0){
              if(status != 0) continue;
              if(ctor->getName().startswith(ctorName)){
                std::vector<Value*> actuals(0);
                InstInsertPt out = InstInsertPt::Before(inst);

                Value* voidPointer = ConstantPointerNull::get(Type::getInt8PtrTy(M.getContext()));
                Value* deviceAddr = Casting::castTo(ci->getArgOperand(0),voidPointer,out,&dataLayout);

                actuals.resize(1);
                actuals[0] = deviceAddr;
                out << CallInst::Create(RegisterDevice,actuals,"");
              }
            }
            else{
              int status = 0;
              //char* ctor_demangled = abi::__cxa_demangle(ctor->getName().data(),0,0,&status);
              //if(ctor_demangled == NULL) continue;
              //DEBUG(errs() << "\n\nctor demangled " << ctor_demangled << "\n\n");

              //if(strcmp(ctorName.data(),ctor->getName().data()) == 0){
              if(status != 0) continue;
              if(ctor_v->getName().startswith(ctorName)){
                std::vector<Value*> actuals(0);
                InstInsertPt out = InstInsertPt::Before(inst);

                Value* voidPointer = ConstantPointerNull::get(Type::getInt8PtrTy(M.getContext()));
                Value* deviceAddr = Casting::castTo(ci->getArgOperand(0),voidPointer,out,&dataLayout);

                actuals.resize(1);
                actuals[0] = deviceAddr;
                out << CallInst::Create(RegisterDevice,actuals,"");
              }

            }
            //else{
            //DEBUG(errs() << "call inst different name " << ctor->getName().data() <<"\n");
            //}
          }
          else if(InvokeInst* ii = dyn_cast<InvokeInst>(inst)){
            //InvokeInst* ci = (InvokeInst*)inst;
            // ii->dump();
            Function* ctor = ii->getCalledFunction();
            //if(ctor == NULL) continue;
            //if(ctor->isDeclaration()) continue;
            Value* ctor_ = ii->getCalledValue();
            //DEBUG(errs() << "\n\n\norginal function name " << ctor_->getName().data() << "\n\n\n");
            //DEBUG(errs() << "invoke inst operands " << ctorName.data() << " / " << ctor->getName().data() << "\n\n\n");
            //printf("\n");
            //        if(strcmp(ctorName.data(),ctor->getName().data()) == 0){
            int status = 0;
            //char* ctor_demangled = abi::__cxa_demangle(ctor_->getName().data(),0,0,&status);
            //DEBUG(errs() << "\n\nctor demangled " << ctor_demangled << "\n\n");
            //if(status != 0) continue;
            if(ctor_->getName().startswith(createConstructorName())){
              //DEBUG(errs() << "\n\nDEBUG::::::::::::::::::::::::::::::::::::::::invoke instruction is matched\n\n");

              std::vector<Value*> actuals(0);
              InstInsertPt out = InstInsertPt::Before(inst);

              Value* voidPointer = ConstantPointerNull::get(Type::getInt8PtrTy(M.getContext()));
              Value* deviceAddr = Casting::castTo(ii->getArgOperand(0),voidPointer,out,&dataLayout);

              actuals.resize(1);
              actuals[0] = deviceAddr;
              out << CallInst::Create(RegisterDevice,actuals,"");
            }
            //else{
            //  DEBUG(errs() << "invoke inst different name " << ctor->getName().data() <<"\n");
            //}
          } */
        }
      }
		}	
	}

	StringRef DeviceLinker::createConstructorName(){
		std::string devName = DeviceName.data();
		int size = DeviceName.size();
		char nameSize[3];
		sprintf(nameSize,"%d",size);
		std::string nameLength = std::string(nameSize);
		std::string pre = "_ZN" + nameLength;
		std::string post = "C";
		std::string finalName = pre + devName + post;
		return StringRef(finalName);
	}
	/*
	void InstMarker::markFunctionInst(Module& M){
		typedef Module::iterator FF;
		typedef Function::iterator BB;
		typedef BasicBlock::iterator II;
		
		EspInitializer& database = getAnalysis< EspInitializer >();

		for(FF FI = M.begin(),FE = M.end();FI !=FE; ++FI){
			Function* F = (Function*) &*FI;
			if (F->isDeclaration()) continue;

			for(BB BI = F->begin(),BE = F->end();BI != BE; ++BI){
				BasicBlock* B = (BasicBlock*) &*BI;
				for(II Ii = B->begin(),IE = B->end();Ii != IE; ++Ii){
					Instruction* inst = (Instruction*) &*Ii;
					if(!isa<CallInst>(inst)) continue;
					CallInst* callInst = cast<CallInst>(inst);
					Function* calledFunction = callInst->getCalledFunction();
					if(calledFunction != nullptr){
						if(callInst->hasMetadata()) continue;
						StringRef functionName = getFunctionNameInFunction(calledFunction->getName());
						//printf("functionName : %s\n",calledFunction->getName());
						if(functionName.size() ==0) continue;
						//printf("DEBUG :: function print -> %s\n",functionName.data());
						//printf("DEBUG :: class Name in function %s : %s\n",className.c_str(),calledFunction->getName().data());
						StringRef deviceName = database.MDTable.getDeviceName(StringRef("function"),StringRef(functionName));
						if(deviceName.size() == 0) continue;
						int deviceID = database.DITable.getDeviceID(deviceName);
						//printf("DEBUG :: deviceID = %d\n",deviceID);
						makeMetadata(inst,deviceID);	
					}
					
				}
			}
		}
	}

	*/	
}
