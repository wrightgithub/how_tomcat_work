package xyy.test.classloader.load;

/**
 * Created by xyy on 16-8-17.
 */
public class ClassLoaderTree {

    public static void testClassIdentity() {
        String classDataRootPath = "/home/xyy/idea-pro/tomcat_work/target/test-classes";
        String classDataRootPath2 = "/home/xyy/idea-pro/tomcat_work/myApp/lib1";
        FileSystemClassLoader fscl1 = new FileSystemClassLoader(true, classDataRootPath);
        FileSystemClassLoader fscl2 = new FileSystemClassLoader(true, classDataRootPath);
        FileSystemClassLoader fscl3 = new FileSystemClassLoader(true, classDataRootPath2);
        boolean ret = (fscl1.equals(fscl2));

        // 前缀必须与类的package的路径一致，否则会报NoClassDefFoundError
        String className = "xyy.test.classloader.load.Sample";
        try {
            Class<?> class1 = fscl1.loadClass(className);
            Object obj1 = class1.newInstance();
            Class<?> class2 = fscl2.loadClass(className);
            Object obj2 = class2.newInstance();
            Class<?> class3 = fscl3.loadClass(className);
            Object obj3 = class3.newInstance();

            /**
             * Sample 和 obj1所用的类加载器不一样
             */
            System.out.println("class1:"+class1.getClassLoader());
            System.out.println("class2:"+class2.getClassLoader());
            System.out.println("class3:"+class3.getClassLoader());
//            System.out.println("Sample.class"+Sample.class.getClassLoader());

            /**
             * 两个不同的Sample的不同的toString,你可以先编译两份不一样的Sample类。
             */
            System.out.println(obj1.toString());
            System.out.println(obj3.toString());

            /**
             * obj1 cannot be cast to test.classload.Sample
             */
            System.out.println("-------------------");
            try {
//                Sample sample = (Sample) obj1;
            } catch (Exception e) {
                System.out.println(e.getMessage());
            }

            /**
             * obj1 cannot be cast to boj2 because first load Sample by AppClassLoader
             */
            System.out.println("-------------------");
            try {
//                obj1=(Sample)obj2;
//                Method setSampleMethod = class1.getMethod("setSample", java.lang.Object.class);
//                setSampleMethod.invoke(obj1, obj2);
            } catch (Exception e) {
               // e.printStackTrace();
                System.out.println(e.getMessage());
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void testClass2(){
        String lib1 = "/home/xyy/idea-pro/tomcat_work/myApp/lib1";
        String lib2 = "/home/xyy/idea-pro/tomcat_work/myApp/lib2";
        String lib3 = "/home/xyy/idea-pro/tomcat_work/myApp/lib3";
        FileSystemClassLoader fscl1 = new FileSystemClassLoader(lib1,"fscl1");
        FileSystemClassLoader fscl2 = new FileSystemClassLoader(fscl1, lib2,"fscl2");
        FileSystemClassLoader fscl3 = new FileSystemClassLoader(lib3,"fscl3");
        FileSystemClassLoader fscl4 = new FileSystemClassLoader(null,lib3,"fscl4");
        String className = "xyy.test.classloader.load.Sample";
        try {
            Class<?> class1= fscl1.loadClass(className);
            class1.newInstance();

            Class<?> class2= fscl2.loadClass(className);
            class2.newInstance();

            Class<?> class3= fscl3.loadClass(className);
            class3.newInstance();


            Class<?> class4= null;
            try {
                class4 = fscl4.loadClass(className);
                class4.newInstance();
            } catch (Exception e) {
                System.out.println("class4: "+e);
            }

            System.out.println("class1’ClassLoader:"+class1.getClassLoader());
            System.out.println("class2’ClassLoader:"+class2.getClassLoader());
            System.out.println("class3’ClassLoader:"+class3.getClassLoader());
            System.out.println("class4’ClassLoader:"+(class4==null?null:class4.getClassLoader()));
        } catch (Exception e) {
            e.printStackTrace();
        }

    }
    public static void main(String[] args) {
         //testAll();
       // testClassIdentity();
        testClass2();
    }

    private static void testAll() {
        ClassLoader loader = ClassLoaderTree.class.getClassLoader();
        while (loader != null) {
            System.out.println(loader.toString());
            loader = loader.getParent();
        }
    }
}
