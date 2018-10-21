
#pragma once

// comma list
#define KKCList0(P)
#define KKCList1(P)  P##1
#define KKCList2(P)  P##1, P##2
#define KKCList3(P)  P##1, P##2, P##3
#define KKCList4(P)  KKCList3(P), P##4
#define KKCList5(P)  KKCList4(P), P##5
#define KKCList6(P)  KKCList5(P), P##6
#define KKCList7(P)  KKCList6(P), P##7
#define KKCList8(P)  KKCList7(P), P##8
#define KKCList9(P)  KKCList8(P), P##9
#define KKCList10(P) KKCList9(P), P##10
#define KKCList11(P) KKCList10(P), P##11
#define KKCList12(P) KKCList11(P), P##12
#define KKCList13(P) KKCList12(P), P##13
#define KKCList14(P) KKCList13(P), P##14
#define KKCList15(P) KKCList14(P), P##15
#define KKCList16(P) KKCList15(P), P##16
#define KKCList(P, N) KKCList##N(P)

// prefix comma list
#define KKPreCList0(R, P)  R
#define KKPreCList1(R, P)  R, P##1
#define KKPreCList2(R, P)  R, P##1, P##2
#define KKPreCList3(R, P)  KKPreCList2(R,P), P##3
#define KKPreCList4(R, P)  KKPreCList3(R,P), P##4
#define KKPreCList5(R, P)  KKPreCList4(R,P), P##5
#define KKPreCList6(R, P)  KKPreCList5(R,P), P##6
#define KKPreCList7(R, P)  KKPreCList6(R,P), P##7
#define KKPreCList8(R, P)  KKPreCList7(R,P), P##8
#define KKPreCList9(R, P)  KKPreCList8(R,P), P##9
#define KKPreCList10(R, P) KKPreCList9(R,P), P##10
#define KKPreCList11(R, P) KKPreCList10(R,P), P##11
#define KKPreCList12(R, P) KKPreCList11(R,P), P##12
#define KKPreCList13(R, P) KKPreCList12(R,P), P##13
#define KKPreCList14(R, P) KKPreCList13(R,P), P##14
#define KKPreCList15(R, P) KKPreCList14(R,P), P##15
#define KKPreCList16(R, P) KKPreCList15(R,P), P##16
#define KKPreCList(R, P, N) KKPreCList##N(R, P)

// suffix comma list
#define KKSufCList0(P, S)  S
#define KKSufCList1(P, S)  KKCList1(P), S
#define KKSufCList2(P, S)  KKCList2(P), S
#define KKSufCList3(P, S)  KKCList3(P), S
#define KKSufCList4(P, S)  KKCList4(P), S
#define KKSufCList5(P, S)  KKCList5(P), S
#define KKSufCList6(P, S)  KKCList6(P), S
#define KKSufCList7(P, S)  KKCList7(P), S
#define KKSufCList8(P, S)  KKCList8(P), S
#define KKSufCList9(P, S)  KKCList9(P), S
#define KKSufCList10(P, S)  KKCList10(P), S
#define KKSufCList11(P, S)  KKCList11(P), S
#define KKSufCList12(P, S)  KKCList12(P), S
#define KKSufCList13(P, S)  KKCList13(P), S
#define KKSufCList14(P, S)  KKCList14(P), S
#define KKSufCList15(P, S)  KKCList15(P), S
#define KKSufCList16(P, S)  KKCList16(P), S
#define KKSufCList(P,S,N) KKSufCList##N(P,S)

// double comma list
#define KKDCList0(P,p)
#define KKDCList1(P,p)  P##1 p##1
#define KKDCList2(P,p)  P##1 p##1, P##2 p##2
#define KKDCList3(P,p)  KKDCList2(P,p), P##3 p##3
#define KKDCList4(P,p)  KKDCList3(P,p), P##4 p##4
#define KKDCList5(P,p)  KKDCList4(P,p), P##5 p##5
#define KKDCList6(P,p)  KKDCList5(P,p), P##6 p##6
#define KKDCList7(P,p)  KKDCList6(P,p), P##7 p##7
#define KKDCList8(P,p)  KKDCList7(P,p), P##8 p##8
#define KKDCList9(P,p)  KKDCList8(P,p), P##9 p##9
#define KKDCList10(P,p) KKDCList9(P,p), P##10 p##10
#define KKDCList11(P,p) KKDCList10(P,p), P##11 p##11
#define KKDCList12(P,p) KKDCList11(P,p), P##12 p##12
#define KKDCList13(P,p) KKDCList12(P,p), P##13 p##13
#define KKDCList14(P,p) KKDCList13(P,p), P##14 p##14
#define KKDCList15(P,p) KKDCList14(P,p), P##15 p##15
#define KKDCList16(P,p) KKDCList15(P,p), P##16 p##16
#define KKDCList(P, p, N)  KKDCList##N(P,p)

// prefix double comma list
#define KKPreDCList0(R,P,p)  R
#define KKPreDCList1(R,P,p)  R, P##1 p##1
#define KKPreDCList2(R,P,p)  R, P##1 p##1, P##2 p##2
#define KKPreDCList3(R,P,p)  KKPreDCList2(R,P,p), P##3 p##3
#define KKPreDCList4(R,P,p)  KKPreDCList3(R,P,p), P##4 p##4
#define KKPreDCList5(R,P,p)  KKPreDCList4(R,P,p), P##5 p##5
#define KKPreDCList6(R,P,p)  KKPreDCList5(R,P,p), P##6 p##6
#define KKPreDCList7(R,P,p)  KKPreDCList6(R,P,p), P##7 p##7
#define KKPreDCList8(R,P,p)  KKPreDCList7(R,P,p), P##8 p##8
#define KKPreDCList9(R,P,p)  KKPreDCList8(R,P,p), P##9 p##9
#define KKPreDCList10(R,P,p) KKPreDCList9(R,P,p), P##10 p##10
#define KKPreDCList11(R,P,p) KKPreDCList10(R,P,p), P##11 p##11
#define KKPreDCList12(R,P,p) KKPreDCList11(R,P,p), P##12 p##12
#define KKPreDCList13(R,P,p) KKPreDCList12(R,P,p), P##13 p##13
#define KKPreDCList14(R,P,p) KKPreDCList13(R,P,p), P##14 p##14
#define KKPreDCList15(R,P,p) KKPreDCList14(R,P,p), P##15 p##15
#define KKPreDCList16(R,P,p) KKPreDCList15(R,P,p), P##16 p##16
#define KKPreDCList(R,P,p,N) KKPreDCList##N(R,P,p)

// double semi-comma list
#define KKDSCList0(P,p)
#define KKDSCList1(P,p)  P##1 p##1
#define KKDSCList2(P,p)  P##1 p##1; P##2 p##2
#define KKDSCList3(P,p)  KKDSCList2(P,p); P##3 p##3
#define KKDSCList4(P,p)  KKDSCList3(P,p); P##4 p##4
#define KKDSCList5(P,p)  KKDSCList4(P,p); P##5 p##5
#define KKDSCList6(P,p)  KKDSCList5(P,p); P##6 p##6
#define KKDSCList7(P,p)  KKDSCList6(P,p); P##7 p##7
#define KKDSCList8(P,p)  KKDSCList7(P,p); P##8 p##8
#define KKDSCList9(P,p)  KKDSCList8(P,p); P##9 p##9
#define KKDSCList10(P,p) KKDSCList9(P,p); P##10 p##10
#define KKDSCList11(P,p) KKDSCList10(P,p); P##11 p##11
#define KKDSCList12(P,p) KKDSCList11(P,p); P##12 p##12
#define KKDSCList13(P,p) KKDSCList12(P,p); P##13 p##13
#define KKDSCList14(P,p) KKDSCList13(P,p); P##14 p##14
#define KKDSCList15(P,p) KKDSCList14(P,p); P##15 p##15
#define KKDSCList16(P,p) KKDSCList15(P,p); P##16 p##16
#define KKDSCList(P, p, N)  KKDSCList##N(P,p)

// macro defined comma list
#define KKMCList0(P)
#define KKMCList1(P)  P(1)
#define KKMCList2(P)  P(1), P(2)
#define KKMCList3(P)  P(1), P(2), P(3)
#define KKMCList4(P)  KKMCList3(P), P(4)
#define KKMCList5(P)  KKMCList4(P), P(5)
#define KKMCList6(P)  KKMCList5(P), P(6)
#define KKMCList7(P)  KKMCList6(P), P(7)
#define KKMCList8(P)  KKMCList7(P), P(8)
#define KKMCList9(P)  KKMCList8(P), P(9)
#define KKMCList10(P) KKMCList9(P), P(10)
#define KKMCList11(P) KKMCList10(P), P(11)
#define KKMCList12(P) KKMCList11(P), P(12)
#define KKMCList13(P) KKMCList12(P), P(13)
#define KKMCList14(P) KKMCList13(P), P(14)
#define KKMCList15(P) KKMCList14(P), P(15)
#define KKMCList16(P) KKMCList15(P), P(16)
#define KKMCList(P, N) KKMCList##N(P)

// prefix macro defined comma list
#define KKPreMCList0(R, P)  R
#define KKPreMCList1(R, P)  R, P(1)
#define KKPreMCList2(R, P)  R, P(1), P(2)
#define KKPreMCList3(R, P)  KKPreMCList2(R,P), P(3)
#define KKPreMCList4(R, P)  KKPreMCList3(R,P), P(4)
#define KKPreMCList5(R, P)  KKPreMCList4(R,P), P(5)
#define KKPreMCList6(R, P)  KKPreMCList5(R,P), P(6)
#define KKPreMCList7(R, P)  KKPreMCList6(R,P), P(7)
#define KKPreMCList8(R, P)  KKPreMCList7(R,P), P(8)
#define KKPreMCList9(R, P)  KKPreMCList8(R,P), P(9)
#define KKPreMCList10(R, P) KKPreMCList9(R,P), P(10)
#define KKPreMCList11(R, P) KKPreMCList10(R,P), P(10)
#define KKPreMCList12(R, P) KKPreMCList11(R,P), P(11)
#define KKPreMCList13(R, P) KKPreMCList12(R,P), P(12)
#define KKPreMCList14(R, P) KKPreMCList13(R,P), P(13)
#define KKPreMCList15(R, P) KKPreMCList14(R,P), P(14)
#define KKPreMCList16(R, P) KKPreMCList15(R,P), P(15)
#define KKPreMCList(R, P, N) KKPreMCList##N(R, P)
