package xyy.test.classloader.load;

/**
 * Created by xyy on 16-8-17.
 */
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

public class FileSystemClassLoader extends ClassLoader {

    private String  loaderName = "FileSystemClassLoader";
    private String  rootDir;
    private boolean localFirst = true;

    public FileSystemClassLoader(boolean localFirst, String rootDir) {
        this.rootDir = rootDir;
        this.localFirst = localFirst;
    }

    public FileSystemClassLoader(String rootDir, String loaderName) {
        super();// parent 设为Application ClassLoader
        this.rootDir = rootDir;
        this.loaderName = loaderName;
    }

    public FileSystemClassLoader(ClassLoader parent, String rootDir, String loaderName) {
        super(parent);
        this.rootDir = rootDir;
        this.loaderName = loaderName;
    }

    @Override
    public Class<?> loadClass(String name) throws ClassNotFoundException {
        if (localFirst) {

            // 找不到时，用父类的loadClass
            Class clazz = findClass(name);
            if (clazz != null) {
                return clazz;
            }

            return super.loadClass(name);
        } else {
            return super.loadClass(name);
        }
    }

    protected Class<?> findClass(String name) {
        byte[] classData = getClassData(name);
        if (classData == null) {
            return null;
        } else {
            return defineClass(name, classData, 0, classData.length);
        }
    }

    private byte[] getClassData(String className) {
        String path = classNameToPath(className);
        try {
            System.out.println(path);
            InputStream ins = new FileInputStream(path);
            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            int bufferSize = 4096;
            byte[] buffer = new byte[bufferSize];
            int bytesNumRead = 0;
            while ((bytesNumRead = ins.read(buffer)) != -1) {
                baos.write(buffer, 0, bytesNumRead);
            }
            return baos.toByteArray();
        } catch (IOException e) {
            // e.printStackTrace();
            // 这里由于如果本次找不到是会报异常的，所以catch掉，然后往上找classloader
            // System.out.println("in getClassData error: " + className);
        }
        return null;
    }

    private String classNameToPath(String className) {
        return rootDir + File.separatorChar + className.replace('.', File.separatorChar) + ".class";
    }

    @Override
    public String toString() {
        return loaderName;
    }
}
