package ex03.pyrmont.startup;

import ex03.pyrmont.connector.http.HttpConnector;

import java.util.Locale;

public final class Bootstrap {
  public static void main(String[] args) {
//    Locale.setDefault(Locale.JAPAN);
    HttpConnector connector = new HttpConnector();
    connector.start();
  }
}
