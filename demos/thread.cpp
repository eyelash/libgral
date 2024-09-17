#include <gral_thread.hpp>

struct Foo {
	void hello() {}
};

Foo foo;

int main() {
	auto thread = gral::Thread::create<Foo, &Foo::hello>(&foo);
}
