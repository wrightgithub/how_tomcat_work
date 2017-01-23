package xyy.test.protected_test.pkg2;

import xyy.test.protected_test.pkg1.Cookie;

/**
 * Created by xyy on 17-1-22.
 */
/**
 * 需要支持Cloneable接口才能clone
 */
public class TestCookie extends Cookie implements Cloneable {

    private String name;

    public TestCookie(String name) {
        this.name = name;
    }

    public void test() {
        say();
    }

    @Override
    public Object clone() throws CloneNotSupportedException {
        return super.clone();
    }

    public static void main(String[] args) throws CloneNotSupportedException {

        // -----------比较以下两种---------------

        // new Cookie().say();// error  不同包只能通过继承得到，直接调用是不可见的。

        new TestCookie("test0").say();// 子类不同包，可以

        // -----------比较以下两种---------------
        TestCookie test1 = new TestCookie("test1");
        test1.test();
        TestCookie test2 = (TestCookie) test1.clone();
        test2.test();

        // 验证两个对象是不同引用
        System.out.println(test1 == test2);
    }
}
