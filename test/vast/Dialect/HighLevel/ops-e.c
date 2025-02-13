// RUN: vast-cc --ccopts -xc --from-source %s | FileCheck %s
// RUN: vast-cc --ccopts -xc --from-source %s > %t && vast-opt %t | diff -B %t -

void assign_assign() {
    int a, b, c;
    // CHECK: [[A:%[0-9]+]] = hl.var "a" : !hl.lvalue<!hl.int>
    // CHECK: [[B:%[0-9]+]] = hl.var "b" : !hl.lvalue<!hl.int>
    // CHECK: [[C:%[0-9]+]] = hl.var "c" : !hl.lvalue<!hl.int>

    // CHECK: [[RA:%[0-9]+]] = hl.ref [[A]] : !hl.lvalue<!hl.int>
    // CHECK: [[V1:%[0-9]+]] = hl.expr
    // CHECK:   [[RB:%[0-9]+]] = hl.ref [[B]] : !hl.lvalue<!hl.int>
    // CHECK:   [[RC:%[0-9]+]] = hl.ref [[C]] : !hl.lvalue<!hl.int>
    // CHECK:   [[V2:%[0-9]+]] = hl.implicit_cast [[RC]] LValueToRValue : !hl.lvalue<!hl.int> -> !hl.int
    // CHECK:   [[VA:%[0-9]+]] = hl.assign [[V2]] to [[RB]] : !hl.int
    // CHECK:  [[VA:%[0-9]+]] = hl.assign [[V1]] to [[RA]] : !hl.int
    a = (b = c);
}
