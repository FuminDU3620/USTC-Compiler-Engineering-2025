#include "DeadCode.hpp"
#include "Instruction.hpp"
#include "logging.hpp"
#include <vector>

// 处理流程：两趟处理，mark 标记有用变量，sweep 删除无用指令
void DeadCode::run() {
    bool changed{};
    func_info->run();
    do {
        changed = false;
        for (auto &F : m_->get_functions()) {
            auto func = &F;
            changed |= clear_basic_blocks(func);
            mark(func);
            changed |= sweep(func);
        }
    } while (changed);
    LOG_INFO << "dead code pass erased " << ins_count << " instructions";
}

bool DeadCode::clear_basic_blocks(Function *func) {
    bool changed = 0;
    std::vector<BasicBlock *> to_erase;
    for (auto &bb1 : func->get_basic_blocks()) {
        auto bb = &bb1;
        if(bb->get_pre_basic_blocks().empty() && bb != func->get_entry_block()) {
            to_erase.push_back(bb);
            changed = 1;
        }
    }
    for (auto &bb : to_erase) {
        bb->erase_from_parent();
        delete bb;
    }
    return changed;
}

void DeadCode::mark(Function *func) {
    // 重置工作列表和标记集合
    work_list.clear();
    marked.clear();
    
    // 标记所有关键指令
    for (auto &bb : func->get_basic_blocks()) {
        for (auto &inst : bb.get_instructions()) {
            if (is_critical(&inst)) {
                mark(&inst);
            }
        }
    }
    
    // 处理工作列表，传播标记
    while (!work_list.empty()) {
        auto inst = work_list.front();
        work_list.pop_front();
        mark(inst);
    }
}

void DeadCode::mark(Instruction *ins) {
    // 如果已经标记过，跳过
    if (marked[ins]) {
        return;
    }
    
    // 标记当前指令
    marked[ins] = true;
    
    // 将操作数加入工作列表（如果操作数是指令）
    for (auto op : ins->get_operands()) {
        if (auto op_inst = dynamic_cast<Instruction*>(op)) {
            work_list.push_back(op_inst);
        }
    }
}

bool DeadCode::sweep(Function *func) {
    // TODO: 删除无用指令
    // 提示：
    // 1. 遍历函数的基本块，删除所有标记为true的指令
    // 2. 删除指令后，可能会导致其他指令的操作数变为无用，因此需要再次遍历函数的基本块
    // 3. 如果删除了指令，返回true，否则返回false
    // 4. 注意：删除指令时，需要先删除操作数的引用，然后再删除指令本身
    // 5. 删除指令时，需要注意指令的顺序，不能删除正在遍历的指令
    std::unordered_set<Instruction *> wait_del{};

    // 1. 收集所有未被标记的指令
    for (auto &bb : func->get_basic_blocks()) {
        for (auto &inst : bb.get_instructions()) {
            if (!marked[&inst]) {
                wait_del.insert(&inst);
            }
        }
    }

    // 2. 执行删除
    for (auto inst : wait_del) {
        // 删除指令的所有使用关系
        inst->remove_all_operands();
        
        // 从基本块中删除指令
        inst->get_parent()->erase_instr(inst);
        
        // 更新统计计数
        ins_count++;
        
        // 删除指令对象
        delete inst;
    }
    
    return not wait_del.empty(); // changed
}

bool DeadCode::is_critical(Instruction *ins) {
    // TODO: 判断指令是否是无用指令
    // 提示：
    // 1. 如果是函数调用，且函数是纯函数，则无用
    // 2. 如果是无用的分支指令，则无用
    // 3. 如果是无用的返回指令，则无用
    // 4. 如果是无用的存储指令，则无用
    // 1. 存储指令是关键的（有副作用）
    if (ins->is_store()) {
        return true;
    }
    
    // 2. 返回指令是关键的
    if (ins->is_ret()) {
        return true;
    }
    
    // 3. 分支指令是关键的
    if (ins->is_br()) {
        return true;
    }
    
    // 4. 函数调用是关键的（假设所有调用都有副作用）
    if (ins->is_call()) {
        return true;
    }
    
    // 5. alloca 指令是关键的（内存分配）
    if (ins->is_alloca()) {
        return true;
    }

    return false;
}

void DeadCode::sweep_globally() {
    std::vector<Function *> unused_funcs;
    std::vector<GlobalVariable *> unused_globals;
    for (auto &f_r : m_->get_functions()) {
        if (f_r.get_use_list().size() == 0 and f_r.get_name() != "main")
            unused_funcs.push_back(&f_r);
    }
    for (auto &glob_var_r : m_->get_global_variable()) {
        if (glob_var_r.get_use_list().size() == 0)
            unused_globals.push_back(&glob_var_r);
    }
    // changed |= unused_funcs.size() or unused_globals.size();
    for (auto func : unused_funcs)
        m_->get_functions().erase(func);
    for (auto glob : unused_globals)
        m_->get_global_variable().erase(glob);
}
