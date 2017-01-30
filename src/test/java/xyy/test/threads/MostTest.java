package xyy.test.threads;

/**
 * Created by xyy on 17-1-26.
 */
public class MostTest {

    public static void main(String[] args) {
        for (int i=0;i<100;i++){
            final String  name= Integer.toString(i);
            new Task(name).start();
        }

        while (true);
    }
}


class Task extends Thread{

    public Task(String name) {
     super(name);
    }

    public void run() {
        int i=0;
        while (true){
            i++;
        }
    }
}
