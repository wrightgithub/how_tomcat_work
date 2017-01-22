package xyy.test.classloader.load;

import java.lang.reflect.Method;

/**
 * Created by xyy on 16-8-17.
 */
public class ClassLoaderTree {

    private static String user_dir = System.getProperty("user.dir");

    public static void main(String[] args) {
        // testAll();
        testClassLoader1();
        // testClassLoader2();
    }

    public static void testClassLoader1() {

        String classDataRootPath = user_dir + "/target/test-classes";
        String classDataRootPath2 = user_dir + "/myApp/lib1";
        FileSystemClassLoader fscl1 = new FileSystemClassLoader(true, classDataRootPath);
        FileSystemClassLoader fscl2 = new FileSystemClassLoader(true, classDataRootPath);
        FileSystemClassLoader fscl3 = new FileSystemClassLoader(true, classDataRootPath2);
        boolean ret = (fscl1.equals(fscl2));

        // 前缀必须与类的package的路径一致，否则会报NoClassDefFoundError
        String className = "xyy.test.classloader.load.Sample";
        try {
            Class<?> class1 = fscl1.loadClass(className);
            Object obj1 = class1.newInstance();
            Object obj1_1 = class1.newInstance();
            Class<?> class2 = fscl2.loadClass(className);
            Object obj2 = class2.newInstance();
            Class<?> class3 = fscl3.loadClass(className);
            Object obj3 = class3.newInstance();

            // Sample 和 obj1所用的类加载器不一样
            System.out.println("class1:" + class1.getClassLoader());
            System.out.println("class2:" + class2.getClassLoader());
            System.out.println("class3:" + class3.getClassLoader());
            System.out.println("Sample.class" + Sample.class.getClassLoader());

            // 两个不同的Sample的不同的toString,你可以先编译两份不一样的Sample类。
            System.out.println(obj1.toString());
            System.out.println(obj3.toString());

            // 不同类加载器加载的类，是不同相互强转的
            System.out.println("------test  cast-------------");
            try {
                Sample sample = (Sample) obj1;
            } catch (Exception e) {
                System.err.println(e);
            }

            System.out.println("--------invoke method -----------");
            try {
                // obj1=(Sample)obj2;
                Method method = class1.getMethod("setSample", java.lang.Object.class);
                // will be error
                // method.invoke(obj1, obj2);
                // 正确
                method.invoke(obj1, obj1_1);
            } catch (Exception e) {
                // e.printStackTrace();
                System.err.println(e);
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void testClassLoader2() {
        String lib1 = user_dir + "/myApp/lib1";
        String lib2 = user_dir + "/myApp/lib2";
        String lib3 = user_dir + "/myApp/lib3";
        FileSystemClassLoader fscl1 = new FileSystemClassLoader(lib1, "fscl1");
        FileSystemClassLoader fscl2 = new FileSystemClassLoader(fscl1, lib2, "fscl2");
        FileSystemClassLoader fscl3 = new FileSystemClassLoader(lib3, "fscl3");
        FileSystemClassLoader fscl4 = new FileSystemClassLoader(null, lib3, "fscl4");
        String className = "xyy.test.classloader.load.Sample";
        try {
            Class<?> class1 = fscl1.loadClass(className);
            class1.newInstance();

            Class<?> class2 = fscl2.loadClass(className);
            class2.newInstance();

            Class<?> class3 = fscl3.loadClass(className);
            class3.newInstance();

            Class<?> class4 = null;
            try {
                class4 = fscl4.loadClass(className);
                class4.newInstance();
            } catch (Exception e) {
                System.err.println("class4: " + e);
            }

            System.out.println("class1’ClassLoader:" + class1.getClassLoader());
            System.out.println("class2’ClassLoader:" + class2.getClassLoader());
            System.out.println("class3’ClassLoader:" + class3.getClassLoader());
            System.out.println("class4’ClassLoader:" + (class4 == null ? null : class4.getClassLoader()));
        } catch (Exception e) {
            e.printStackTrace();
        }

    }

    private static void testAll() {
        ClassLoader loader = ClassLoaderTree.class.getClassLoader();
        while (loader != null) {
            System.out.println(loader.toString());
            loader = loader.getParent();
        }
    }
}
