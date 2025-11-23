#include "../../include/passes/FunctionInline.hpp"
#include "../../include/lightir/Function.hpp"

#include "BasicBlock.hpp"
#include "Instruction.hpp"
#include "Value.hpp"
#include "logging.hpp"
#include <cassert>
#include <utility>
#include <vector>

void FunctionInline::run() { inline_all_functions(); }

void FunctionInline::inline_all_functions() {
    
    std::set<Function *> recursive_func;
    for (auto &func : m_->get_functions()) {
        for (auto &bb : func.get_basic_blocks()) {
            for (auto &inst : bb.get_instructions()) {
                if (inst.is_call()) {
                    auto call = &inst;
                    auto func1 = static_cast<Function *>(call->get_operand(0));
                    if (func1 == &func) {
                        recursive_func.insert(func1);
                        break;
                    }
                }
            }
        }
    }
    for (auto &func : m_->get_functions()) {
        if (outside_func.find(func.get_name()) != outside_func.end()) {
            continue;
        }
    a1:
        for (auto &bb : func.get_basic_blocks()) {
            for (auto &inst : bb.get_instructions()) {
                if (inst.is_call()) {
                    auto call = &inst;
                    auto func1 = static_cast<Function *>(call->get_operand(0));
                    if (func1 == &func) {
                        continue;
                    }
                    if (recursive_func.find(func1) != recursive_func.end())
                        continue;
                    if (outside_func.find(func1->get_name()) !=
                        outside_func.end())
                        continue;
                    if(func1->get_basic_blocks().size() >=6){
                        continue;
                    }
                    inline_function(call, func1);
                    goto a1;
                }
            }
        }
    }
}

void FunctionInline::inline_function(Instruction *call, Function *origin) {
    std::map<Value *, Value *> v_map;
    std::vector<BasicBlock *> bb_list;
    std::vector<Instruction *> ret_list; // 记录函数所有出口（克隆后的 return 指令）
    for (auto &arg : origin->get_args()) {
        v_map.insert(std::make_pair(static_cast<Value *>(&arg),
                                    call->get_operand(arg.get_arg_no() + 1)));
    }
    auto call_bb = call->get_parent();
    auto call_func = call_bb->get_parent();
    std::vector<BasicBlock *> ret_void_bbs;

    // 克隆被调用函数的基本块与指令（建立 v_map）
    for (auto &bb : origin->get_basic_blocks()) {
        auto bb_new =
            BasicBlock::create(call_func->get_parent(), "", call_func);
        v_map.insert(std::make_pair(static_cast<Value *>(&bb),
                                    static_cast<Value *>(bb_new)));
        bb_list.push_back(bb_new);
        for (auto &inst : bb.get_instructions()) {
            if (inst.is_ret() && origin->get_return_type()->is_void_type()) {
                // 对于 void 返回，记录其所在的克隆 basic block，稍后把它们直接连到后续基本块
                ret_void_bbs.push_back(bb_new);
                continue;
            }
            if (inst.is_phi()) {
                ; // phi 会被克隆到 bb 新建时作为开始指令（见下面）
            }

            Instruction *inst_new;
            if (inst.is_call()) {
                auto call = static_cast<CallInst *>(&inst);
                auto func = static_cast<Function *>(call->get_operand(0));
                inst_new = new CallInst(func, {call->get_operands().begin() + 1, call->get_operands().end()}, bb_new);
            } else
                inst_new = inst.clone(bb_new);

            if (inst.is_phi())
                bb_new->add_instr_begin(inst_new);

            v_map.insert(std::make_pair(static_cast<Value *>(&inst),
                                        static_cast<Value *>(inst_new)));
            if (inst.is_ret()) {
                // 保存克隆后的 return 指令以便后续处理
                ret_list.push_back(inst_new);
            }
        }
    }

    // 替换克隆后指令的操作数（使用 v_map）
    for (auto bb : bb_list) {
        for (auto &inst : bb->get_instructions()) {
            for (int i = 0; i < inst.get_num_operand(); i++) {
                if (inst.is_phi()) {
                    ; // phi 的 incoming 在创建时需要特殊处理；不过这里克隆来的 phi 已经在 clone 中设置（若需另外处理可加）
                }
                auto op = inst.get_operand(i);
                if (v_map.find(op) != v_map.end()) {
                    inst.set_operand(i, v_map[op]);
                }
            }
        }
    }

    Value *ret_val = nullptr; // 返回值（若函数非 void）
    bool is_terminated = false;
    auto bb_new = BasicBlock::create(call_func->get_parent(), "", call_func); // 后续继续执行的基本块（inlined 后的接续点）

    if (!origin->get_return_type()->is_void_type()) {
        if (ret_list.size() == 1) {
            // 单返回点：直接使用其返回值并把该返回点改为跳转到 bb_new
            auto ret = ret_list.front();
            ret_val = ret->get_operand(0);
            auto ret_bb = ret->get_parent();
            // 移除 return 指令，改为跳转到 bb_new
            ret_bb->remove_instr(ret);
            BranchInst::create_br(bb_new, ret_bb);
        } else {
            // 多返回点：创建一个 bb_phi，用 phi 指令合并各返回路径的返回值
            auto bb_phi = BasicBlock::create(call_func->get_parent(), "", call_func);
            // 使用 create_phi 创建一个空的 phi（没有 incoming）
            PhiInst *phi = PhiInst::create_phi(origin->get_return_type(), bb_phi, {}, {});
            // 对于每个克隆的 return 指令：
            for (auto ret : ret_list) {
                // ret 是克隆后的 return 指令，取其返回操作数（已用 v_map 替换为克隆内部的值或传入值）
                Value *val = ret->get_operand(0);
                BasicBlock *pred = ret->get_parent();
                // 移除 return 指令，并在其原基本块上添加跳转到 bb_phi
                pred->remove_instr(ret);
                BranchInst::create_br(bb_phi, pred);
                // 将 (val, pred) 添加为 phi 的 incoming
                // add_phi_pair_operand 的签名是 (Value* val, Value* pre_bb)
                // BasicBlock 继承自 Value*，因此可以直接传 pred
                phi->add_phi_pair_operand(val, pred);
            }
            // 把 phi 指令放在 bb_phi 的开头
            bb_phi->add_instr_begin(phi);
            // bb_phi 最终要跳转到 bb_new（接续点）
            BranchInst::create_br(bb_new, bb_phi);
            // 将 bb_phi 加入克隆基本块列表（以便后续可能的处理或重置）
            bb_list.push_back(bb_phi);
            // phi 为内联函数的返回值
            ret_val = phi;
        }
    } else {
        // void 函数：所有原函数中的 return（克隆到 ret_void_bbs）都跳转到 bb_new
        assert(ret_void_bbs.size() > 0);
        for (auto bb : ret_void_bbs) {
            BranchInst::create_br(bb_new, bb);
        }
    }

    std::vector<Instruction *> del_list;
    BranchInst* br = nullptr;

    // 在调用点所在基本块中，找到 call 指令并插入从 call_bb 到 inlined entry 的分支，
    // 并将调用点之后的指令移动到新建的 bb_new 中（call 被移除）
    for (auto &inst : call_bb->get_instructions()) {
        if (!is_terminated) {
            if (&(inst) == call) {
                // 在调用位置插入跳转到被内联函数的入口（克隆后的第一个基本块）
                br = BranchInst::create_br(bb_list.front(), call_bb);
                if (!origin->get_return_type()->is_void_type()) {
                    // 把调用结果的使用者都替换为内联返回值
                    call->replace_all_use_with(ret_val);
                }
                is_terminated = true;
            }
        } else {
            // 调用之后的指令都需要移动到 bb_new（除了刚插入的那条分支）
            if(dynamic_cast<BranchInst*>(&inst) == br){
                continue;
            }
            del_list.push_back(&inst);
        }
    }

    // 移除 call 指令本体
    call_bb->remove_instr(call);
    origin->remove_use(call, 0);

    // 将 call 之后的指令移动到 bb_new（保持执行顺序）
    for (auto inst : del_list) {
        call_bb->remove_instr(inst);
        bb_new->add_instruction(inst);
        inst->set_parent(bb_new);
    }

    // 重新构建被调用函数与调用函数的 basic-block 列表（可能用于后续分析/遍历）
    origin->reset_bbs();
    call_func->reset_bbs();

    return;
}
