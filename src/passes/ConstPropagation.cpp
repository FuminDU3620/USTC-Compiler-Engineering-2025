#include "ConstPropagation.hpp"

#include "Instruction.hpp"
#include "logging.hpp"

ConstantInt *ConstFolder::compute(Instruction::OpID op, ConstantInt *value1, ConstantInt *value2) {
    int c_value1 = value1->get_value();
    int c_value2 = value2->get_value();

    switch (op) {
    case Instruction::add:
        return ConstantInt::get(c_value1 + c_value2, module_);
        break;
    case Instruction::sub:
        return ConstantInt::get(c_value1 - c_value2, module_);
        break;
    case Instruction::mul:
        return ConstantInt::get(c_value1 * c_value2, module_);
        break;
    case Instruction::sdiv:
        return ConstantInt::get(static_cast<int>(c_value1 / c_value2), module_);
        break;
    case Instruction::eq:
        return ConstantInt::get(c_value1 == c_value2, module_);
        break;
    case Instruction::ne:
        return ConstantInt::get(c_value1 != c_value2, module_);
        break;
    case Instruction::gt:
        return ConstantInt::get(c_value1 > c_value2, module_);
        break;
    case Instruction::ge:
        return ConstantInt::get(c_value1 >= c_value2, module_);
        break;
    case Instruction::lt:
        return ConstantInt::get(c_value1 < c_value2, module_);
        break;
    case Instruction::le:
        return ConstantInt::get(c_value1 <= c_value2, module_);
        break;
    default:
        return nullptr;
        break;
    }
}

ConstantFP *ConstFolder::compute(Instruction::OpID op, ConstantFP *value1, ConstantFP *value2) {
    float c_value1 = value1->get_value();
    float c_value2 = value2->get_value();
    switch (op) {
    case Instruction::fadd:
        return ConstantFP::get(c_value1 + c_value2, module_);
        break;
    case Instruction::fsub:
        return ConstantFP::get(c_value1 - c_value2, module_);
        break;
    case Instruction::fmul:
        return ConstantFP::get(c_value1 * c_value2, module_);
        break;
    case Instruction::fdiv:
        return ConstantFP::get(c_value1 / c_value2, module_);
        break;
    case Instruction::feq:
        return nullptr;
        break;
    case Instruction::fne:
        return nullptr;
        break;
    case Instruction::fgt:
        return nullptr;
        break;
    case Instruction::fge:
        return nullptr;
        break;
    case Instruction::flt:
        return nullptr;
        break;
    case Instruction::fle:
        return nullptr;
        break;
    default:
        return nullptr;
        break;
    }
}
ConstantFP *ConstFolder::compute(Instruction::OpID op, ConstantInt *value1) {
    int c_value1 = value1->get_value();

    switch (op) {
    case Instruction::sitofp:
        return ConstantFP::get((float) c_value1, module_);
        break;

    default:
        return nullptr;
        break;
    }
}

ConstantInt *ConstFolder::compute(Instruction::OpID op, ConstantFP *value1) {
    float c_value1 = value1->get_value();
    switch (op) {
    case Instruction::fptosi:
        return ConstantInt::get(static_cast<int>(c_value1), module_);
        break;

    default:
        return nullptr;
        break;
    }
}

ConstantFP *cast_constantfp(Value *value) {
    auto constant_fp_ptr = dynamic_cast<ConstantFP *>(value);
    if (constant_fp_ptr) {
        return constant_fp_ptr;
    }
    return nullptr;
}
ConstantInt *cast_constantint(Value *value) {
    auto constant_int_ptr = dynamic_cast<ConstantInt *>(value);
    if (constant_int_ptr) {
        return constant_int_ptr;
    }
    return nullptr;
}

void ConstPropagation::run() {
    for (auto &func : m_->get_functions()) {
        for (auto &bb : func.get_basic_blocks()) {
            wait_delete.clear();
            globalvar_def.clear();

            for (auto &instr : bb.get_instructions()) {
                // 处理整数二元运算
                if (instr.is_add() || instr.is_sub() || instr.is_mul() || instr.is_div()) {
                    auto value1 = cast_constantint(instr.get_operand(0));
                    auto value2 = cast_constantint(instr.get_operand(1));
                    if (value1 && value2) {
                        auto fold_const = folder->compute(instr.get_instr_type(), value1, value2);
                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                }
                // 处理浮点二元运算
                else if (instr.is_fadd() || instr.is_fsub() || instr.is_fmul() || instr.is_fdiv()) {
                    auto value1 = cast_constantfp(instr.get_operand(0));
                    auto value2 = cast_constantfp(instr.get_operand(1));
                    if (value1 && value2) {
                        auto fold_const = folder->compute(instr.get_instr_type(), value1, value2);
                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                }
                // 处理整数比较指令
                else if (instr.is_cmp()) {
                    auto value1 = cast_constantint(instr.get_operand(0));
                    auto value2 = cast_constantint(instr.get_operand(1));
                    if (value1 && value2) {
                        auto fold_const = folder->compute(instr.get_instr_type(), value1, value2);
                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                }
                // 处理浮点比较指令
                else if (instr.is_fcmp()) {
                    auto value1 = cast_constantfp(instr.get_operand(0));
                    auto value2 = cast_constantfp(instr.get_operand(1));
                    if (value1 && value2) {
                        auto fold_const = folder->compute(instr.get_instr_type(), value1, value2);
                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                }
                // 处理类型转换指令: float -> int
                else if (instr.is_fp2si()) {
                    auto value1 = cast_constantfp(instr.get_operand(0));
                    if (value1) {
                        auto fold_const = folder->compute(instr.get_instr_type(), value1);
                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                }
                // 处理类型转换指令: int -> float
                else if (instr.is_si2fp()) {
                    auto value1 = cast_constantint(instr.get_operand(0));
                    if (value1) {
                        auto fold_const = folder->compute(instr.get_instr_type(), value1);
                        instr.replace_all_use_with(fold_const);
                        wait_delete.push_back(&instr);
                    }
                }
            }

            for (auto instr : wait_delete) {
                bb.erase_instr(instr);
            }
        }
    }

    for (auto &func : m_->get_functions()) {
        for (auto &bb : func.get_basic_blocks()) {
            builder->set_insert_point(&bb);
            
            // 检查条件分支的条件是否为常量
            auto terminator = bb.get_terminator();
            if (terminator && terminator->is_br()) {
                auto br_inst = static_cast<BranchInst*>(terminator);
                if (br_inst->is_cond_br()) {
                    auto cond = br_inst->get_condition();
                    auto const_cond = cast_constantint(cond);
                    
                    if (const_cond) {
                        // 根据常量条件值选择分支
                        BasicBlock* target_bb = nullptr;
                        if (const_cond->get_value() != 0) {
                            // 条件为真，跳转到true分支
                            target_bb = static_cast<BasicBlock*>(br_inst->get_operand(1));
                        } else {
                            // 条件为假，跳转到false分支  
                            target_bb = static_cast<BasicBlock*>(br_inst->get_operand(2));
                        }
                        
                        // 创建无条件分支指令
                        auto new_br = BranchInst::create_br(target_bb, &bb);
                        
                        // 删除原来的条件分支指令
                        bb.erase_instr(br_inst);
                        
                        // 记录需要删除的基本块（不可达的基本块）
                        BasicBlock* unreachable_bb = nullptr;
                        if (const_cond->get_value() != 0) {
                            unreachable_bb = static_cast<BasicBlock*>(br_inst->get_operand(2));
                        } else {
                            unreachable_bb = static_cast<BasicBlock*>(br_inst->get_operand(1));
                        }
                        
                        if (unreachable_bb) {
                            delete_bb.push_back(unreachable_bb);
                        }
                    }
                }
            }
        }
        
        for (auto bb : delete_bb) {
            clear_blocks_recs(bb);
        }
        delete_bb.clear();
    }
}

bool ConstPropagation::is_entry(BasicBlock *bb) {
    auto func = bb->get_parent();
    if (func == nullptr) {
        return false;
    }
    // 函数的第一个基本块就是入口块
    return &(*func->get_basic_blocks().begin()) == bb;
}

void ConstPropagation::clear_blocks_recs(BasicBlock *start_bb) {
    auto func = start_bb->get_parent();
    if (func == nullptr) {
        LOG(ERROR) << "basic block-" << start_bb->get_name() << " has no parent function";
    } else {
        auto prev_bb = start_bb->get_pre_basic_blocks();
        // start_bb has no previous bb and is not the entry of parent function
        if (prev_bb.size() == 0 && !is_entry(start_bb)) {
            func->remove(start_bb);
            auto succ_bb = start_bb->get_succ_basic_blocks();
            for (auto each_succ_bb : succ_bb) {
                std::vector<Instruction*> del_inst;
                for (auto &instr1 : each_succ_bb->get_instructions()) {
                    auto instr = &instr1;
                    if (instr->is_phi()) {
                        LOG(DEBUG) << "Find a PHI instruction in the sucess node of "
                                      "useless branch";
                        for (int i = 1; i < instr->get_num_operand(); i += 2) {
                            if (instr->get_operand(i) == start_bb &&
                                start_bb->get_pre_basic_blocks().size() <= 0) {
                                LOG(DEBUG) << "remove unuseful phi branch in the index of " << i - 1
                                           << " and " << i;

                                instr->remove_operand(i - 1);
                                instr->remove_operand(i - 1);
                            }
                        }
                        int operands_num_phi = instr->get_num_operand();
                        if (operands_num_phi == 2) {
                            auto value = instr->get_operand(0);
                            instr->replace_all_use_with(cast_constantint(value));
                            del_inst.push_back(instr);
                        }
                    }
                }
                for(auto instr : del_inst) each_succ_bb->erase_instr(instr);
                clear_blocks_recs(each_succ_bb);
            }
        }
    }
}
