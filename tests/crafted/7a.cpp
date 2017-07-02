typedef int number;
typedef int n2;

template <class T> T foo() {
  return 0;
}

// test comment pls ignore
class Foo {
  void f1() { ; }

  void f2() { ; }

  void f3() { ; }

  void f5() { ; }

  bool b = true;

  void f6() { ; }

};

class Bar {
  void b1() { ; }

  void b4() { ; }

  void b5() { ; }

  int x;

  void b2() { ; }

  void b3() { ; }

  void b6() { ; }
};

class Choo {
  void c2() { ; }

  void c3() { ; }

  void c4() { ; }

  void c5() { ; }

  void c1() { ; }

  void c6() { ; }

};

int a = 1;
int b = a + 2;
int c = b - 3;
int a1 = 1;
int b1 = a + 2;
int c1 = b - 3;
int a2 = 1;
int b2 = a + 2;
int c2 = b - 3;
int a3 = 1;
int b3 = a + 2;
int c3 = b - 3;

int x = a + b + c + a1 + b1 + c1 + a2 + b2 + c2;
