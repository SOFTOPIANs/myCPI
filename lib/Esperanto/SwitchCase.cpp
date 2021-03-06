#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"

#include "corelab/Utilities/IniFini.h"
#include "corelab/Utilities/Casting.h"
#include "corelab/Utilities/InstInsertPt.h"
//#include "corelab/Metadata/NamedMetadata.h"
#include "corelab/Esperanto/InstMarker.h"
#include "corelab/Esperanto/EspInit.h"
#include "corelab/Esperanto/EspUtils.h"
#include "corelab/Esperanto/RemoteCall.h"
#include "corelab/Esperanto/SwitchCase.h"
#include "corelab/Esperanto/MainFcnCreator.h"

#include <iostream>

namespace corelab {
  using namespace llvm;
  using namespace std;

  static cl::opt<string> GVInitializer("gv_initializer",cl::desc("Specify Global variable initializer (1: gv_initializer, 0: not)"),cl::value_desc("global initializer in charge"));
  
  static RegisterPass<SwitchCase> X("switch-case", "Switch-Case Test for Esperanto", false, false);
  char SwitchCase::ID = 0;

  void SwitchCase::getAnalysisUsage (AnalysisUsage &AU) const {
    // AU.addRequired< LoadNamer >();
    AU.addRequired< EspInitializer >();
    AU.addRequired< InstMarker >();
    AU.addRequired< RemoteCall >();
    AU.addRequired< MainCreator >();
    AU.setPreservesAll ();
  }

  bool SwitchCase::runOnModule (Module& M) {
    LLVMContext& Context = M.getContext();
    RemoteCall& remoteCall = getAnalysis< RemoteCall >(); 
    EspInitializer& espInit = getAnalysis< EspInitializer >();
    MainCreator& mc = getAnalysis< MainCreator >();
    // EsperantoNamer& en = getAnalysis<EsperantoNamer>();

    /*** Insert Switch-Case Function ***/
    setFunctions(M);
    Function* execFunction = createExecFunction(M);
    
    /*** Insert "Init" Function ***/
    Function* targetFcn = getOrInsertConstructor(M);
    Instruction* targetInst = targetFcn->front().getFirstNonPHI();
    
      

    std::vector<Value*> actuals(3);
    actuals[0] = execFunction;
    Value* devID = ConstantInt::get(Type::getInt32Ty(Context), espInit.DITable.getDeviceID(remoteCall.deviceName),1);
    actuals[1] = devID;

    if(strcmp(GVInitializer.data(),"1") == 0)
      actuals[2] = ConstantInt::get(Type::getInt32Ty(Context),1);
    else
      actuals[2] = ConstantInt::get(Type::getInt32Ty(Context),0);

    CallInst::Create(Init, actuals, "", targetInst);

    //actuals.resize(1);
    //actuals[0] = (Value*)(new LoadInst(GV,"",initInst)); 
    //CallInst::Create(Debug, actuals, "", initInst);

    std::vector<Function*> removes = mc.functionToRemove;
    for(int i=0;i<removes.size();i++)
      removes[i]->eraseFromParent();


    //FIXME: Insert init function with this callback function
    //std::vector<Value*> actuals(2);
    //Function *initFunction = M.getFunction("init");
    //CallInst::Create(execFunction, actuals, "", &iniFunction->front());
    return true;
  }

  void SwitchCase::setFunctions(Module& M) { 
    LLVMContext& Context = M.getContext();
    RemoteCall& remoteCall = getAnalysis<RemoteCall>(); 
    // void* consume(int jobid)i;

    Debug = (Function*) M.getOrInsertFunction(
        "debugAddress", 
        Type::getVoidTy(Context), 
        Type::getInt8PtrTy(Context),
        (Type*) 0);

    Consume = (Function*) M.getOrInsertFunction(
        "consumeFunctionArgs",
        Type::getInt8PtrTy(Context),
        Type::getInt32Ty(Context),
        (Type*) 0);

    // void produceReturn(int jobId, void* buf, int size);
    Produce = (Function*) M.getOrInsertFunction(
        "produceReturn",
        Type::getVoidTy(Context),
        Type::getInt32Ty(Context),
        Type::getInt8PtrTy(Context),
        Type::getInt32Ty(Context),
        (Type*) 0);

    
    /**** Set "Init" Function ****/
    std::vector<Type*> args;
    args.push_back(IntegerType::get(Context, 32));
    args.push_back(IntegerType::get(Context, 32));
    FunctionType* callback = FunctionType::get(
        /*Result=*/Type::getVoidTy(Context),
        /*Params=*/args,
        /*isVarArg=*/false);
    PointerType* callbackPointer = PointerType::get(callback, 0);
    std::vector<Type*> formals(3);
    formals[0] = callbackPointer;
    formals[1] = Type::getInt32Ty(Context);
    formals[2] = Type::getInt32Ty(Context);

    FunctionType *initFunc = FunctionType::get(Type::getVoidTy(Context),formals,false);

    string functionName; // char functionName[50];
		functionName = "EspInit";

    Init = (Function*) M.getOrInsertFunction(functionName, initFunc);
  }

  Function* SwitchCase::createExecFunction(Module& M) {
    LLVMContext& Context = M.getContext();
    const DataLayout& dataLayout = M.getDataLayout();

    // LoadNamer& loadNamer = getAnalysis< LoadNamer >();
    EspInitializer& espInit = getAnalysis< EspInitializer >();
    RemoteCall& remoteCall = getAnalysis< RemoteCall >();
		InstMarker& instMarker = getAnalysis< InstMarker >();
    // EsperantoNamer& esperantoNamer = getAnalysis< EsperantoNamer >();

    std::vector<Type*> formals(2);
    formals[0] = Type::getInt32Ty(Context);
    formals[1] = Type::getInt32Ty(Context);
    FunctionType* voidFcnIntType = FunctionType::get(Type::getVoidTy(Context), formals, false);
    Function* execFunction = Function::Create(voidFcnIntType, GlobalValue::InternalLinkage, "EsperantoExecFunction", &M);

    Function::arg_iterator ai = execFunction->arg_begin();
    //Function::ArgumentListType& args = execFunction->getArgumentList();
    //Value* ai = (Value)(args.begin());

    Value* argFID = (Value*) &*ai;
    Value* argJID = (Value*) &*(++ai);

    // Set basic blocks 
    BasicBlock* execBody = BasicBlock::Create(Context, "execBody", execFunction);
    BasicBlock* execExit = BasicBlock::Create(Context, "execExit", execFunction);

    // Create base of switch instruction
    SwitchInst* switchFID = SwitchInst::Create(argFID, execExit, 1, execBody);
    ReturnInst::Create(Context, 0, execExit);

    std::vector<Value*> actualJID(1);
    actualJID[0] = argJID;

    std::vector<Value*> actuals(0);

    for(Module::iterator fi = M.begin(); fi != M.end(); ++fi){
      Function* fp = (Function*)fi;
      if (!remoteCall.localFunctionTable[fp])  
        continue;
      
      int myFID = espInit.functionTable.getFunctionID(fp);
      BasicBlock* execCase = BasicBlock::Create(Context, "execCase", execFunction);
      switchFID->addCase(ConstantInt::get(Type::getInt32Ty(Context), myFID), execCase);

      int argIndex = 0;
      int addressSum = 0;
      actuals.resize(fi->arg_size());
      Type* integerType = IntegerType::get(Context, dataLayout.getPointerSizeInBits());

      actualJID.resize(1);
      // 'consumed' buffer pointer to integer type
      Value* consumed = CallInst::Create(Consume, actualJID, "", execCase);
      InstInsertPt out = InstInsertPt::End(execCase);
      Value* tempInt = Constant::getNullValue(integerType);
      //Value* bufInt = EspUtils::insertCastingBefore(consumed, tempInt, &dataLayout, execCase->getTerminator());
      Value* bufInt = Casting::castTo(consumed, tempInt, out, &dataLayout);

      // Consume each arguments
      // bool classMember = false;
      for(Function::arg_iterator ai = fi->arg_begin(); ai != fi->arg_end(); ai++){

        Type* argType = ai->getType();
        if(!argType->isPointerTy()){

         // printf("handle normal argument - switchcase\n");
          int argSize = dataLayout.getTypeAllocSizeInBits(argType);
          // add target pointer addres
          Value* bufAddress = BinaryOperator::CreateAdd(bufInt, ConstantInt::get(integerType, addressSum/8), "", execCase);
          Value *tempPtr = ConstantPointerNull::get(argType->getPointerTo(0));
          out = InstInsertPt::End(execCase);
          Value *argPtr = Casting::castTo(bufAddress, tempPtr, out, &dataLayout);
          actuals[argIndex++] = new LoadInst(argPtr, "", execCase);
          addressSum += argSize;
        }
        else{
          //printf("handle pointer argument - switchcase\n");
          unsigned pointerSize = dataLayout.getPointerTypeSize(argType);
          //printf("pointer size is %d byte\n",(int)pointerSize);
          PointerType* pointerType;
          pointerType = dyn_cast<PointerType>(argType); 

          //Type* newIntegerType = IntegerType::get(Context, 32);
          Value* bufAddress = BinaryOperator::CreateAdd(bufInt, ConstantInt::get(integerType, addressSum/8), "", execCase);
          //bufAddress->dump();
          Value *tempPtr = ConstantPointerNull::get(Type::getInt32PtrTy(Context));
          out = InstInsertPt::End(execCase);
          Value *argPtr = Casting::castTo(bufAddress, tempPtr, out, &dataLayout);
          //argPtr->dump();
          Value* addrIn32 = new LoadInst(argPtr,"",execCase);
          //addrIn32->dump();
          //Value* addrIn64 = Casting::castTo(addrIn32, ConstantInt::get(Type::getInt64Ty(Context),0), out, &dataLayout);
          //addrIn64->dump();
          out = InstInsertPt::End(execCase);
          Value* realAddr = Casting::castTo(addrIn32, ConstantPointerNull::get(pointerType), out, &dataLayout);
          //realAddr->dump();
          actuals[argIndex++] = realAddr;
          addressSum += 32;
          //Value* newArg = Casting::castTo((Value*)&*ai,
        }
      }

      /**** Insert Function Call ****/
      Value* ret = (Value*) CallInst::Create((Value*)fp, actuals, "", execCase); 

      // Produce its return value
      if(fi->getReturnType()->isVoidTy()){
        // case: return type is void
        actualJID.resize(3);

        Value* voidptr = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
        actualJID[1] = voidptr;
        actualJID[2] = ConstantInt::get(Type::getInt32Ty(Context), 0);

        CallInst::Create(Produce, actualJID, "", execCase);
      } else {
        // case: return type is not void
        Type* retType = fi->getReturnType();
        int retSize = dataLayout.getTypeAllocSize(fi->getReturnType());
        actualJID.resize(3); 

        Value* one = ConstantInt::get(Type::getInt32Ty(Context), 1); 
        AllocaInst* alloca = new AllocaInst(retType, one, "", execCase);
        new StoreInst(ret, alloca, execCase);
        Value* voidptr = ConstantPointerNull::get(Type::getInt8PtrTy(Context));
        InstInsertPt out = InstInsertPt::End(execCase);
        actualJID[1] = Casting::castTo(alloca, voidptr, out, &dataLayout);
        //actualJID[1] = EspUtils::insertCastingBefore(alloca, voidptr, &dataLayout, execCase->getTerminator());
        actualJID[2] = ConstantInt::get(Type::getInt32Ty(Context), retSize);
        CallInst::Create(Produce, actualJID, "", execCase);
      }
      BranchInst::Create(execExit, execCase);
    }

    return execFunction;
  }

}
