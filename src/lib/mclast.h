#pragma once

#include "mctoken.h"
#include "mcheap.h"

typedef enum ParseNodeType {
   PN_NIL,
   PN_TRUE,
   PN_FALSE,
   PN_INTEGER,
   PN_FLOAT,
   PN_STRING,
   PN_VARARG,
   PN_NAME,

   PN_BINOP,
   PN_UNOP,
   PN_INDEX,
   PN_FIELD,
   PN_CALL,
   PN_CALL_METHOD,
   PN_FUNC_EXPR,
   PN_TABLE,

   PN_FIELD_IDX,
   PN_FIELD_NAME,
   PN_FIELD_VAL,

   PN_EXPLIST,
   PN_NAMELIST,
   PN_VARLIST,

   PN_FUNCBODY,
   PN_ARGLIST,
   PN_ATTNAME,

   PN_BLOCK,
   PN_ASSIGN,
   PN_LOCAL,
   PN_DO,
   PN_WHILE,
   PN_REPEAT,
   PN_IF,
   PN_ELSEIF,
   PN_ELSE,
   PN_FORNUM,
   PN_FORIN,
   PN_FUNCDEF,
   PN_LOCAL_FUNC,
   PN_RETURN,
   PN_BREAK,
   PN_GOTO,
   PN_LABEL,
   PN_CALL_STAT,
} ParseNodeType;

typedef struct mclastnode {
   ParseNodeType type;
   SemanticInfo  val;
   u32 child_count;
} mclastnode;

#define AST(node)            ((node)->object.astnode)
#define AST_TYPE(node)       ((node)->object.astnode->type)
#define AST_VAL(node)        ((node)->object.astnode->val)
#define AST_NCHILD(node)     ((node)->object.astnode->child_count)
#define AST_CHILD(node, idx) (mclast_children(node)[(idx)])
#define AST_CHILD_CAP(node)  (((node)->objcap - sizeof(mclastnode)) / sizeof(heap_header*))

heap_header* mclast_alloc(ParseNodeType type, u32 child_cap, MCHeap* heap);
i8 mclast_push_child(heap_header* parent, heap_header* child, MCHeap* heap);
heap_header** mclast_children(heap_header* node);
