/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This file is included by Codegen-armv5te-vfp.c, and implements architecture
 * variant-specific code.
 */

extern void dvmCompilerFlushRegWideForV5TEVFP(CompilationUnit *cUnit,
                                              int reg1, int reg2);
extern void dvmCompilerFlushRegForV5TEVFP(CompilationUnit *cUnit, int reg);

/* First, flush any registers associated with this value */
static void loadValueAddress(CompilationUnit *cUnit, RegLocation rlSrc,
                             int rDest)
{
     rlSrc = rlSrc.wide ? dvmCompilerUpdateLocWide(cUnit, rlSrc) :
                          dvmCompilerUpdateLoc(cUnit, rlSrc);
     if (rlSrc.location == kLocPhysReg) {
         if (rlSrc.wide) {
             dvmCompilerFlushRegWideForV5TEVFP(cUnit, rlSrc.lowReg,
                                               rlSrc.highReg);
         } else {
             dvmCompilerFlushRegForV5TEVFP(cUnit, rlSrc.lowReg);
         }
     }
     opRegRegImm(cUnit, kOpAdd, rDest, rFP,
                 dvmCompilerS2VReg(cUnit, rlSrc.sRegLow) << 2);
}

static bool genInlineSqrt(CompilationUnit *cUnit, MIR *mir)
{
    RegLocation rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
    RegLocation rlResult = LOC_C_RETURN_WIDE;
    RegLocation rlDest = LOC_DALVIK_RETURN_VAL_WIDE;
    loadValueAddress(cUnit, rlSrc, r2);
    genDispatchToHandler(cUnit, TEMPLATE_SQRT_DOUBLE_VFP);
    storeValueWide(cUnit, rlDest, rlResult);
    return false;
}

/*
 * TUNING: On some implementations, it is quicker to pass addresses
 * to the handlers rather than load the operands into core registers
 * and then move the values to FP regs in the handlers.  Other implementations
 * may prefer passing data in registers (and the latter approach would
 * yeild cleaner register handling - avoiding the requirement that operands
 * be flushed to memory prior to the call).
 */
static bool genArithOpFloat(CompilationUnit *cUnit, MIR *mir,
                            RegLocation rlDest, RegLocation rlSrc1,
                            RegLocation rlSrc2)
{
    TemplateOpCode opCode;

    /*
     * Don't attempt to optimize register usage since these opcodes call out to
     * the handlers.
     */
    switch (mir->dalvikInsn.opCode) {
        case OP_ADD_FLOAT_2ADDR:
        case OP_ADD_FLOAT:
            opCode = TEMPLATE_ADD_FLOAT_VFP;
            break;
        case OP_SUB_FLOAT_2ADDR:
        case OP_SUB_FLOAT:
            opCode = TEMPLATE_SUB_FLOAT_VFP;
            break;
        case OP_DIV_FLOAT_2ADDR:
        case OP_DIV_FLOAT:
            opCode = TEMPLATE_DIV_FLOAT_VFP;
            break;
        case OP_MUL_FLOAT_2ADDR:
        case OP_MUL_FLOAT:
            opCode = TEMPLATE_MUL_FLOAT_VFP;
            break;
        case OP_REM_FLOAT_2ADDR:
        case OP_REM_FLOAT:
        case OP_NEG_FLOAT: {
            return genArithOpFloatPortable(cUnit, mir, rlDest, rlSrc1, rlSrc2);
        }
        default:
            return true;
    }
    loadValueAddress(cUnit, rlDest, r0);
    dvmCompilerClobber(cUnit, r0);
    loadValueAddress(cUnit, rlSrc1, r1);
    dvmCompilerClobber(cUnit, r1);
    loadValueAddress(cUnit, rlSrc2, r2);
    genDispatchToHandler(cUnit, opCode);
    rlDest = dvmCompilerUpdateLoc(cUnit, rlDest);
    if (rlDest.location == kLocPhysReg) {
        dvmCompilerClobber(cUnit, rlDest.lowReg);
    }
    return false;
}

static bool genArithOpDouble(CompilationUnit *cUnit, MIR *mir,
                             RegLocation rlDest, RegLocation rlSrc1,
                             RegLocation rlSrc2)
{
    TemplateOpCode opCode;

    switch (mir->dalvikInsn.opCode) {
        case OP_ADD_DOUBLE_2ADDR:
        case OP_ADD_DOUBLE:
            opCode = TEMPLATE_ADD_DOUBLE_VFP;
            break;
        case OP_SUB_DOUBLE_2ADDR:
        case OP_SUB_DOUBLE:
            opCode = TEMPLATE_SUB_DOUBLE_VFP;
            break;
        case OP_DIV_DOUBLE_2ADDR:
        case OP_DIV_DOUBLE:
            opCode = TEMPLATE_DIV_DOUBLE_VFP;
            break;
        case OP_MUL_DOUBLE_2ADDR:
        case OP_MUL_DOUBLE:
            opCode = TEMPLATE_MUL_DOUBLE_VFP;
            break;
        case OP_REM_DOUBLE_2ADDR:
        case OP_REM_DOUBLE:
        case OP_NEG_DOUBLE: {
            return genArithOpDoublePortable(cUnit, mir, rlDest, rlSrc1,
                                               rlSrc2);
        }
        default:
            return true;
    }
    loadValueAddress(cUnit, rlDest, r0);
    dvmCompilerClobber(cUnit, r0);
    loadValueAddress(cUnit, rlSrc1, r1);
    dvmCompilerClobber(cUnit, r1);
    loadValueAddress(cUnit, rlSrc2, r2);
    genDispatchToHandler(cUnit, opCode);
    rlDest = dvmCompilerUpdateLocWide(cUnit, rlDest);
    if (rlDest.location == kLocPhysReg) {
        dvmCompilerClobber(cUnit, rlDest.lowReg);
        dvmCompilerClobber(cUnit, rlDest.highReg);
    }
    return false;
}

static bool genConversion(CompilationUnit *cUnit, MIR *mir)
{
    OpCode opCode = mir->dalvikInsn.opCode;
    bool longSrc = false;
    bool longDest = false;
    RegLocation rlSrc;
    RegLocation rlDest;
    TemplateOpCode template;
    switch (opCode) {
        case OP_INT_TO_FLOAT:
            longSrc = false;
            longDest = false;
            template = TEMPLATE_INT_TO_FLOAT_VFP;
            break;
        case OP_FLOAT_TO_INT:
            longSrc = false;
            longDest = false;
            template = TEMPLATE_FLOAT_TO_INT_VFP;
            break;
        case OP_DOUBLE_TO_FLOAT:
            longSrc = true;
            longDest = false;
            template = TEMPLATE_DOUBLE_TO_FLOAT_VFP;
            break;
        case OP_FLOAT_TO_DOUBLE:
            longSrc = false;
            longDest = true;
            template = TEMPLATE_FLOAT_TO_DOUBLE_VFP;
            break;
        case OP_INT_TO_DOUBLE:
            longSrc = false;
            longDest = true;
            template = TEMPLATE_INT_TO_DOUBLE_VFP;
            break;
        case OP_DOUBLE_TO_INT:
            longSrc = true;
            longDest = false;
            template = TEMPLATE_DOUBLE_TO_INT_VFP;
            break;
        case OP_LONG_TO_DOUBLE:
        case OP_FLOAT_TO_LONG:
        case OP_LONG_TO_FLOAT:
        case OP_DOUBLE_TO_LONG:
            return genConversionPortable(cUnit, mir);
        default:
            return true;
    }

    if (longSrc) {
        rlSrc = dvmCompilerGetSrcWide(cUnit, mir, 0, 1);
    } else {
        rlSrc = dvmCompilerGetSrc(cUnit, mir, 0);
    }

    if (longDest) {
        rlDest = dvmCompilerGetDestWide(cUnit, mir, 0, 1);
    } else {
        rlDest = dvmCompilerGetDest(cUnit, mir, 0);
    }
    loadValueAddress(cUnit, rlDest, r0);
    dvmCompilerClobber(cUnit, r0);
    loadValueAddress(cUnit, rlSrc, r1);
    genDispatchToHandler(cUnit, template);
    if (rlDest.wide) {
        rlDest = dvmCompilerUpdateLocWide(cUnit, rlDest);
        dvmCompilerClobber(cUnit, rlDest.highReg);
    } else {
        rlDest = dvmCompilerUpdateLoc(cUnit, rlDest);
    }
    dvmCompilerClobber(cUnit, rlDest.lowReg);
    return false;
}

static bool genCmpFP(CompilationUnit *cUnit, MIR *mir, RegLocation rlDest,
                     RegLocation rlSrc1, RegLocation rlSrc2)
{
    TemplateOpCode template;
    RegLocation rlResult = dvmCompilerGetReturn(cUnit);
    bool wide = true;

    switch(mir->dalvikInsn.opCode) {
        case OP_CMPL_FLOAT:
            template = TEMPLATE_CMPL_FLOAT_VFP;
            wide = false;
            break;
        case OP_CMPG_FLOAT:
            template = TEMPLATE_CMPG_FLOAT_VFP;
            wide = false;
            break;
        case OP_CMPL_DOUBLE:
            template = TEMPLATE_CMPL_DOUBLE_VFP;
            break;
        case OP_CMPG_DOUBLE:
            template = TEMPLATE_CMPG_DOUBLE_VFP;
            break;
        default:
            return true;
    }
    loadValueAddress(cUnit, rlSrc1, r0);
    dvmCompilerClobber(cUnit, r0);
    loadValueAddress(cUnit, rlSrc2, r1);
    genDispatchToHandler(cUnit, template);
    storeValue(cUnit, rlDest, rlResult);
    return false;
}
