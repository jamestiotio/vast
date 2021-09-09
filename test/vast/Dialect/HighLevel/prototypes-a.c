// RUN: vast-cc --from-source %s | FileCheck %s

short b(void);
// CHECK: func private @b() -> !hl.short

int c(void);
// CHECK: func private @c() -> !hl.int

long d(void);
// CHECK: func private @d() -> !hl.long

long long e(void);
// CHECK: func private @e() -> !hl.longlong

void f(void);
// CHECK: func private @f() -> !hl.void

void g(int);
// CHECK: func private @g(!hl.int) -> !hl.void

void h(int, int);
// CHECK: func private @h(!hl.int, !hl.int) -> !hl.void