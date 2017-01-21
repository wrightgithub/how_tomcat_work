package xyy.test.classloader.load;

/**
 * Created by xyy on 16-8-17.
 */
public class Sample {
    private Sample instance;

    public void setSample(Object instance) {
        this.instance = (Sample) instance;
    }

    public String toString(){
        return "Sample str1";
    }
}
