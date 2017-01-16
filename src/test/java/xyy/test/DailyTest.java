package xyy.test;

/**
 * Created by xyy on 17-1-16.
 */
public class DailyTest {

    public static void main(String[] args) {
        boolean flag = false;

        loop: {
            if (flag) {
                break loop;
            }

            System.out.println("in loop");
        }

        System.out.println("out loop");

    }
}
