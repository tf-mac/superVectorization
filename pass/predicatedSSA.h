#include "llvm/IR/Value.h"
#include "llvm/IR/Instruction.h"  // Add this
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"     // Add this
#include "llvm/IR/Type.h" 
#include <vector>
#include <string>
#include <variant>
#include <sstream>

struct Item;

struct SSAMuNode {
    std::variant<SSAMuNode*, llvm::Value*> init;
    std::variant<SSAMuNode*, llvm::Value*> rec;
    llvm::Type* type;
};

using SSAValue = std::variant<SSAMuNode*, llvm::Value*>;

struct SSAPredicate {
    enum Kind {
        True,
        Condition,
        Not,
        And,
        Or
    };
    
    Kind kind;
    SSAPredicate* left = nullptr;
    SSAPredicate* right = nullptr;
    llvm::Value* condition;
};

struct SSALoop {
    struct MuBinding {
        std::string variable;
        SSAMuNode* muNode;
    };
    
    std::vector<MuBinding> muBindings;
    std::vector<Item> bodyItems;
    SSAPredicate* whileCondition = nullptr; 
};

struct Item {
    std::variant<llvm::Instruction*, SSALoop*> content;
    SSAPredicate* Predicate = nullptr;
};

struct SSAFunction {
    std::vector<Item> items;
};

SSAFunction* convertToPredicatedSSA(llvm::Function& llvmFunc);
void lowerToIR(SSAFunction* function, llvm::Function& llvmFunc);

class PredicatedSSAPrinter {
public:
    static void print(SSAFunction* function, llvm::raw_fd_ostream& os) {
        os << "Function:\n";
        for ( auto& item : function->items) {
            itemToString(&item, os, 0);
            os << "\n";
        }
    }

private:
    static void itemToString(Item* item, llvm::raw_fd_ostream& os, int loopDepth) {
        // Add indentation for loops
        for (int i = 0; i < loopDepth; i++) {
            os << "  ";
        }
        
        // Print predicate
        os << "[";
        os << predicateToString(item->Predicate);
        os << "] ";
        
        // Print content
        if (std::holds_alternative<llvm::Instruction*>(item->content)) {
            auto* inst = std::get<llvm::Instruction*>(item->content);
            os << "Instruction: " << *inst; 
        } else if (std::holds_alternative<SSALoop*>(item->content)) {
            auto* loop = std::get<SSALoop*>(item->content);
            os << "loop {\n";
            for (auto& bodyItem : loop->bodyItems) {
                itemToString(&bodyItem, os, loopDepth + 1);
                os << "\n";
            }
            for (int i = 0; i < loopDepth; i++) {
                os << "  ";
            }
            os << "}";
            if (loop->whileCondition) {
                os << " while " << predicateToString(loop->whileCondition);
            }
        }
    }

    static std::string predicateToString(SSAPredicate* pred) {
        if (!pred) return "true";
        
        switch (pred->kind) {
            case SSAPredicate::True:
                return "true";
            case SSAPredicate::Condition:
                if (pred->condition && pred->condition->hasName()) {
                    return "%" + pred->condition->getName().str();
                }
                return "cond";
            case SSAPredicate::Not:
                return "!(" + predicateToString(pred->left) + ")";
            case SSAPredicate::And:
                return "(" + predicateToString(pred->left) + " && " + 
                       predicateToString(pred->right) + ")";
            case SSAPredicate::Or:
                return "(" + predicateToString(pred->left) + " || " + 
                       predicateToString(pred->right) + ")";
            default:
                return "unknown";
        }
    }
};