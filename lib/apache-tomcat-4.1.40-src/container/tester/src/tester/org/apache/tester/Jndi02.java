/*
 * Copyright 1999, 2000, 2001 ,2004 The Apache Software Foundation.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.tester;


import java.io.*;
import javax.naming.Binding;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;
import javax.servlet.*;
import javax.servlet.http.*;
import org.apache.tester.SessionBean;
import org.apache.tester.shared.SharedSessionBean;
import org.apache.tester.unpshared.UnpSharedSessionBean;
import org.apache.tester.unshared.UnsharedSessionBean;


/**
 * Positive test for looking up environment entries from the naming context
 * provided by the servlet container.  The looked-up values are initialized
 * via <code>&lt;env-entry&gt;</code> elements in the web application
 * deployment descriptor.
 *
 * @author Craig R. McClanahan
 * @version $Revision: 396182 $ $Date: 2006-04-23 01:07:48 +0100 (Sun, 23 Apr 2006) $
 */

public class Jndi02 extends HttpServlet {

    // Names of the known <env-entry> elements
    String names[] =
    { "booleanEntry", "byteEntry", "doubleEntry", "floatEntry",
      "integerEntry", "longEntry", "stringEntry", "nested" };


    // Reference some application classes for the first time in destroy()
    // and log the results
    public void destroy() {

        try {
            SessionBean sb = new SessionBean();
            log("OK Accessing SessionBean");
        } catch (Throwable t) {
            log("FAIL Accessing SessionBean", t);
        }

        try {
            SharedSessionBean sb = new SharedSessionBean();
            log("OK Accessing SharedSessionBean");
        } catch (Throwable t) {
            log("FAIL Accessing SharedSessionBean", t);
        }

        try {
            UnpSharedSessionBean sb = new UnpSharedSessionBean();
            log("OK Accessing UnpSharedSessionBean");
        } catch (Throwable t) {
            log("FAIL Accessing UnpSharedSessionBean", t);
        }

        try {
            UnsharedSessionBean sb = new UnsharedSessionBean();
            log("OK Accessing UnsharedSessionBean");
        } catch (Throwable t) {
            log("FAIL Accessing UnsharedSessionBean", t);
        }

    }


    public void doGet(HttpServletRequest request, HttpServletResponse response)
        throws IOException, ServletException {

        // Prepare to render our output
        response.setContentType("text/plain");
        PrintWriter writer = response.getWriter();
        StringBuffer sb = new StringBuffer();
        boolean ok = true;
        Object value = null;

        // Look up the initial context provided by our servlet container
        Context initContext = null;
        try {
            initContext = new InitialContext();
        } catch (NamingException e) {
            log("Create initContext", e);
            sb.append("  Cannot create initContext.");
            ok = false;
        }

        // Look up the environment context provided to our web application
        Context envContext = null;
        try {
            if (ok) {
                value = initContext.lookup("java:comp/env");
                envContext = (Context) value;
                if (envContext == null) {
                    sb.append("  Missing envContext.");
                    ok = false;
                }
            }
        } catch (ClassCastException e) {
            sb.append("  envContext class is ");
            sb.append(value.getClass().getName());
            sb.append(".");
            ok = false;
        } catch (NamingException e) {
            log("Create envContext", e);
            sb.append("  Cannot create envContext.");
            ok = false;
        }

        // Validate the booleanEntry environment entry
        try {
            if (ok) {
                value = envContext.lookup("booleanEntry");
                Boolean booleanValue = (Boolean) value;
                if (!(booleanValue.booleanValue() == true)) {
                    sb.append("  booleanValue is ");
                    sb.append(booleanValue);
                    sb.append(".");
                }
            }
        } catch (ClassCastException e) {
            sb.append("  booleanValue class is ");
            sb.append(value.getClass().getName());
            sb.append(".");                      
        } catch (NullPointerException e) {
            sb.append("  booleanValue is missing.");
        } catch (NamingException e) {
            log("Get booleanValue", e);
            sb.append("  Cannot get booleanValue.");
        }

        // Validate the byteEntry environment entry
        try {
            if (ok) {
                value = envContext.lookup("byteEntry");
                Byte byteValue = (Byte) value;
                if (!(byteValue.byteValue() == 123)) {
                    sb.append("  byteValue is ");
                    sb.append(byteValue);
                    sb.append(".");
                }
            }
        } catch (ClassCastException e) {
            sb.append("  byteValue class is ");
            sb.append(value.getClass().getName());
            sb.append(".");                      
        } catch (NullPointerException e) {
            sb.append("  byteValue is missing.");
        } catch (NamingException e) {
            log("Get byteValue", e);
            sb.append("  Cannot get byteValue.");
        }

        // Validate the doubleEntry environment entry
        try {
            if (ok) {
                value = envContext.lookup("doubleEntry");
                Double doubleValue = (Double) value;
                if (!(doubleValue.doubleValue() == 123.45)) {
                    sb.append("  doubleValue is ");
                    sb.append(doubleValue);
                    sb.append(".");
                }
            }
        } catch (ClassCastException e) {
            sb.append("  doubleValue class is ");
            sb.append(value.getClass().getName());
            sb.append(".");                      
        } catch (NullPointerException e) {
            sb.append("  doubleValue is missing.");
        } catch (NamingException e) {
            log("Get doubleValue", e);
            sb.append("  Cannot get doubleValue.");
        }

        // Validate the floatEntry environment entry
        try {
            if (ok) {
                value = envContext.lookup("floatEntry");
                Float floatValue = (Float) value;
                float difference = floatValue.floatValue() - ((float) 54.32);
                if ((difference < ((float) -0.01)) ||
                    (difference > ((float)  0.01))) {
                    sb.append("  floatValue is ");
                    sb.append(floatValue);
                    sb.append(".");
                }
            }
        } catch (ClassCastException e) {
            sb.append("  floatValue class is ");
            sb.append(value.getClass().getName());
            sb.append(".");                      
        } catch (NullPointerException e) {
            sb.append("  floatValue is missing.");
        } catch (NamingException e) {
            log("Get floatValue", e);
            sb.append("  Cannot get floatValue.");
        }

        // Validate the integerEntry environment entry
        try {
            if (ok) {
                value = envContext.lookup("integerEntry");
                Integer integerValue = (Integer) value;
                if (!(integerValue.intValue() == 12345)) {
                    sb.append("  integerValue is ");
                    sb.append(integerValue);
                    sb.append(".");
                }
            }
        } catch (ClassCastException e) {
            sb.append("  integerValue class is ");
            sb.append(value.getClass().getName());
            sb.append(".");                      
        } catch (NullPointerException e) {
            sb.append("  integerValue is missing.");
        } catch (NamingException e) {
            log("Get integerValue", e);
            sb.append("  Cannot get integerValue.");
        }

        // Validate the longEntry environment entry
        try {
            if (ok) {
                value = envContext.lookup("longEntry");
                Long longValue = (Long) value;
                if (!(longValue.longValue() == 54321)) {
                    sb.append("  longValue is ");
                    sb.append(longValue);
                    sb.append(".");
                }
            }
        } catch (ClassCastException e) {
            sb.append("  longValue class is ");
            sb.append(value.getClass().getName());
            sb.append(".");                      
        } catch (NullPointerException e) {
            sb.append("  longValue is missing.");
        } catch (NamingException e) {
            log("Get longValue", e);
            sb.append("  Cannot get longValue.");
        }

        // Validate the stringEntry environment entry
        try {
            if (ok) {
                value = envContext.lookup("stringEntry");
                String stringValue = (String) value;
                if (!"String Value".equals(stringValue)) {
                    sb.append("  stringValue is ");
                    sb.append(stringValue);
                    sb.append(".");
                }
            }
        } catch (ClassCastException e) {
            sb.append("  stringValue class is ");
            sb.append(value.getClass().getName());
            sb.append(".");                      
        } catch (NullPointerException e) {
            sb.append("  stringValue is missing.");
        } catch (NamingException e) {
            log("Get stringValue", e);
            sb.append("  Cannot get stringValue.");
        }

        // Validate the nestedEntry environment entry
        try {
            if (ok) {
                value = envContext.lookup("nested/nestedEntry");
                String stringValue = (String) value;
                if (!"Nested Value".equals(stringValue)) {
                    sb.append("  stringValue is ");
                    sb.append(stringValue);
                    sb.append(".");
                }
            }
        } catch (ClassCastException e) {
            sb.append("  stringValue class is ");
            sb.append(value.getClass().getName());
            sb.append(".");                      
        } catch (NullPointerException e) {
            sb.append("  stringValue is missing.");
        } catch (NamingException e) {
            log("Get stringValue", e);
            sb.append("  Cannot get stringValue.");
        }

        // Validate that we can enumerate the contents of our environment
        try {
            if (ok) {
                int counts[] = new int[names.length];
                for (int i = 0; i < names.length; i++)
                    counts[i] = 0;
                NamingEnumeration enumeration =
                    initContext.listBindings("java:comp/env");
                while (enumeration.hasMore()) {
                    Binding binding = (Binding) enumeration.next();
                    String name = binding.getName();
                    boolean found = false;
                    for (int i = 0; i < names.length; i++) {
                        if (name.equals(names[i])) {
                            counts[i]++;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        StaticLogger.write("Found binding for '" + name + "'");
                }
                for (int i = 0; i < names.length; i++) {
                    if (counts[i] < 1) {
                        sb.append("  Missing binding for ");
                        sb.append(names[i]);
                        sb.append(".");
                    } else if (counts[i] > 1) {
                        sb.append("  Found ");
                        sb.append(counts[i]);
                        sb.append(" bindings for ");
                        sb.append(names[i]);
                        sb.append(".");
                    }
                }
            }
        } catch (NamingException e) {
            log("Enumerate envContext", e);
            sb.append("  Cannot enumerate envContext");
        }

        // Report our ultimate success or failure
        if (sb.length() < 1)
            writer.println("Jndi02 PASSED");
        else {
            writer.print("Jndi02 FAILED -");
            writer.println(sb);
        }

        // Add wrapper messages as required
        while (true) {
            String message = StaticLogger.read();
            if (message == null)
                break;
            writer.println(message);
        }
        StaticLogger.reset();

    }

}
