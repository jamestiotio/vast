// RUN: vast-cc --ccopts -xc --from-source %s | FileCheck %s
// RUN: vast-cc --ccopts -xc --from-source %s > %t && vast-opt %t | diff -B %t -

int f(int), fc(const int);

int main() {
    // CHECK: hl.var "pc" : !hl.lvalue<!hl.ptr<!hl.paren<(!hl.lvalue<!hl.int< const >>) -> !hl.int>>>
    // CHECK:   hl.funcref @f : !hl.lvalue<(!hl.lvalue<!hl.int>) -> !hl.int>
    int (*pc)(const int) = f;
    // CHECK: hl.var "p" : !hl.lvalue<!hl.ptr<!hl.paren<(!hl.lvalue<!hl.int>) -> !hl.int>>>
    // CHECK:   hl.funcref @fc : !hl.lvalue<(!hl.lvalue<!hl.int< const >>) -> !hl.int>
    int (*p)(int) = fc;
    // CHECK: hl.assign [[P:%[0-9]+]] to [[PC:%[0-9]+]] : !hl.ptr<!hl.paren<(!hl.lvalue<!hl.int>) -> !hl.int>>, !hl.lvalue<!hl.ptr<!hl.paren<(!hl.lvalue<!hl.int< const >>) -> !hl.int>>> -> !hl.ptr<!hl.paren<(!hl.lvalue<!hl.int< const >>) -> !hl.int>>
    pc = p;
}
