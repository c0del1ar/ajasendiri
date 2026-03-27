#ifndef AJA_CORE_AST_H
#define AJA_CORE_AST_H

#include "core/token.h"

typedef enum {
    VT_INVALID = 0,
    VT_INT,
    VT_FLOAT,
    VT_STRING,
    VT_ERROR,
    VT_BOOL,
    VT_LIST,
    VT_MAP,
    VT_CHANNEL,
    VT_OBJECT,
    VT_INTERFACE,
    VT_FUNCTION,
    VT_MULTI,
    VT_VOID
} ValueType;

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct FuncDecl FuncDecl;
typedef struct TypeDecl TypeDecl;
typedef struct InterfaceDecl InterfaceDecl;

typedef struct TypeRef TypeRef;
typedef struct MultiTypeSig MultiTypeSig;

typedef struct {
    TypeRef *params;
    int param_count;
    int param_cap;
    TypeRef *return_type;
} FuncTypeSig;

struct MultiTypeSig {
    TypeRef *items;
    int count;
    int cap;
};

struct TypeRef {
    ValueType kind;
    char *custom_name;
    FuncTypeSig *func_sig;
    MultiTypeSig *multi_sig;
    TypeRef *list_elem_type;
    TypeRef *map_key_type;
    TypeRef *map_value_type;
    TypeRef *chan_elem_type;
};

typedef enum {
    EX_INT = 0,
    EX_FLOAT,
    EX_STRING,
    EX_BOOL,
    EX_LIST,
    EX_MAP,
    EX_LIST_COMP,
    EX_MAP_COMP,
    EX_IDENT,
    EX_INDEX,
    EX_SLICE,
    EX_UNARY,
    EX_BINARY,
    EX_LAMBDA,
    EX_CALL
} ExprKind;

struct Expr {
    ExprKind kind;
    int line;
    union {
        long long int_val;
        double float_val;
        char *string_val;
        int bool_val;
        struct {
            Expr **items;
            int count;
            int cap;
        } list_lit;
        struct {
            char **keys;
            Expr **values;
            int count;
            int cap;
        } map_lit;
        struct {
            Expr *item_expr;
            char *var_name;
            Expr *iterable;
            Expr *cond;
            int has_cond;
        } list_comp;
        struct {
            Expr *key_expr;
            Expr *value_expr;
            char *var_name;
            Expr *iterable;
            Expr *cond;
            int has_cond;
        } map_comp;
        char *ident_name;
        struct {
            Expr *target;
            Expr *index;
        } index;
        struct {
            Expr *target;
            Expr *start;
            Expr *end;
            int has_start;
            int has_end;
        } slice;
        struct {
            TokenType op;
            Expr *expr;
        } unary;
        struct {
            Expr *left;
            TokenType op;
            Expr *right;
        } binary;
        struct {
            FuncDecl *fn;
        } lambda;
        struct {
            Expr *callee;
            Expr **args;
            char **arg_names;
            int arg_count;
            int arg_cap;
        } call;
    } as;
};

typedef enum {
    ST_ASSIGN = 0,
    ST_MULTI_ASSIGN,
    ST_CONST,
    ST_INDEX_ASSIGN,
    ST_FIELD_ASSIGN,
    ST_INC,
    ST_BREAK,
    ST_CONTINUE,
    ST_DEFER,
    ST_KOSTROUTINE,
    ST_RETURN,
    ST_EXPR,
    ST_IF,
    ST_MATCH,
    ST_SELECT,
    ST_FOR,
    ST_WHILE,
    ST_DO_WHILE
} StmtKind;

typedef enum {
    FOR_MODE_RANGE = 0,
    FOR_MODE_EACH
} ForMode;

typedef struct {
    Expr *cond;
    Stmt **body;
    int body_count;
    int body_cap;
    int line;
} IfBranch;

typedef struct {
    Expr *pattern;
    Stmt **body;
    int body_count;
    int body_cap;
    int line;
} MatchCase;

typedef enum {
    SELECT_CASE_RECV = 0,
    SELECT_CASE_SEND,
    SELECT_CASE_TIMEOUT
} SelectCaseKind;

typedef struct {
    SelectCaseKind kind;
    Expr *op_call;
    char *bind_name;
    int has_bind;
    Stmt **body;
    int body_count;
    int body_cap;
    int line;
} SelectCase;

struct Stmt {
    StmtKind kind;
    int line;
    union {
        struct {
            char *name;
            Expr *expr;
        } assign;
        struct {
            char **names;
            int name_count;
            int name_cap;
            Expr *expr;
        } multi_assign;
        struct {
            char *name;
            Expr *expr;
        } const_assign;
        struct {
            char *name;
            Expr *index;
            Expr *expr;
        } index_assign;
        struct {
            char *obj_name;
            char *field_name;
            Expr *expr;
        } field_assign;
        struct {
            char *name;
        } inc;
        struct {
            char *label;
            int has_label;
        } break_stmt;
        struct {
            char *label;
            int has_label;
        } continue_stmt;
        struct {
            Expr *expr;
        } defer_stmt;
        struct {
            Expr *expr;
        } kostroutine_stmt;
        struct {
            Expr **values;
            int value_count;
            int value_cap;
        } ret;
        struct {
            Expr *expr;
        } expr_stmt;
        struct {
            IfBranch *branches;
            int branch_count;
            int branch_cap;
            Stmt **else_body;
            int else_count;
            int else_cap;
            int has_else;
        } if_stmt;
        struct {
            Expr *target;
            MatchCase *cases;
            int case_count;
            int case_cap;
            Stmt **default_body;
            int default_count;
            int default_cap;
            int has_default;
        } match_stmt;
        struct {
            SelectCase *cases;
            int case_count;
            int case_cap;
            Stmt **default_body;
            int default_count;
            int default_cap;
            int has_default;
        } select_stmt;
        struct {
            ForMode mode;
            char *var_name;
            char *var_name2;
            int has_second_var;
            char *loop_label;
            Expr *start;
            Expr *end;
            Expr *step;
            Expr *iterable;
            Stmt **body;
            int body_count;
            int body_cap;
        } for_stmt;
        struct {
            char *loop_label;
            Expr *cond;
            Stmt **body;
            int body_count;
            int body_cap;
        } while_stmt;
        struct {
            char *loop_label;
            Stmt **body;
            int body_count;
            int body_cap;
            Expr *cond;
        } do_while_stmt;
    } as;
};

typedef struct {
    char *name;
    TypeRef type;
} FieldDecl;

struct TypeDecl {
    char *name;
    FieldDecl *fields;
    int field_count;
    int field_cap;
    int line;
};

typedef struct {
    char *name;
    TypeRef type;
    Expr *default_expr;
    int is_kw_only;
    int line;
} Param;

typedef struct {
    char *name;
    Param *params;
    int param_count;
    int param_cap;
    TypeRef return_type;
    int line;
} InterfaceMethodSig;

struct InterfaceDecl {
    char *name;
    InterfaceMethodSig *methods;
    int method_count;
    int method_cap;
    int line;
};

typedef enum {
    IMPORT_ALL = 0,
    IMPORT_SELECTIVE
} ImportMode;

typedef struct {
    ImportMode mode;
    char *module;
    char *alias;
    char **names;
    int name_count;
    int name_cap;
} ImportDecl;

struct FuncDecl {
    char *name;
    int has_receiver;
    Param receiver;
    Param *params;
    int param_count;
    int param_cap;
    TypeRef return_type;
    Stmt **body;
    int body_count;
    int body_cap;
    int line;
};

typedef struct {
    TypeDecl **types;
    int type_count;
    int type_cap;
    InterfaceDecl **interfaces;
    int interface_count;
    int interface_cap;
    FuncDecl **funcs;
    int func_count;
    int func_cap;
    Stmt **stmts;
    int stmt_count;
    int stmt_cap;
    ImportDecl *imports;
    int import_count;
    int import_cap;
    char **exports;
    int export_count;
    int export_cap;
} Program;

#endif
