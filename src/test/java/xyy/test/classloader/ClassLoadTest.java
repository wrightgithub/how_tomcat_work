package xyy.test.classloader;

import java.io.IOException;
import java.io.InputStream;

/**
 * Created by xyy on 17-1-21.
 */
class MyLoader extends ClassLoader {

    @Override
    public Class<?> loadClass(String name) throws ClassNotFoundException {
        try {
            // 找到对应路径的.class文件
            String filename = name.substring(name.lastIndexOf(".") + 1) + ".class";
            InputStream is = getClass().getResourceAsStream(filename);
            if (is == null) {
                return super.loadClass(name);
            }

            byte[] b = new byte[is.available()];
            is.read(b);
            return defineClass(name, b, 0, b.length);
        } catch (IOException e) {
            throw new ClassNotFoundException(name);
        }

    }
}

public class ClassLoadTest {

    public static void main(String[] args) throws Exception {
        ClassLoader myloader = new MyLoader();

        String className = "xyy.test.classloader.ClassLoadTest";
        Object object = myloader.loadClass(className).newInstance();

        Object object1 = new ClassLoadTest();
        System.out.println(object.getClass());
        System.out.println(object.getClass().getClassLoader());
        System.out.println(object1.getClass().getClassLoader());
        System.out.println(object instanceof ClassLoadTest);
    }
}
