public class Test {

public static void main(String argv[]) {
	for (int i = 0; i < 100; i++) {
		foo(3,4);
		foo(-3,-4);
	}
}

public static void foo(int x, int y) {
	if (x < 0 && y < 0) {
		foo(y, x+1);
	}	
}

}
