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
import javax.servlet.*;
import javax.servlet.http.*;

/**
 * Testing for double forwarding.
 *
 * @author Craig R. McClanahan
 * @version $Revision: 289023 $ $Date: 2004-08-26 23:06:34 +0100 (Thu, 26 Aug 2004) $
 */

public class Forward07 extends HttpServlet {

    public void doGet(HttpServletRequest request, HttpServletResponse response)
        throws IOException, ServletException {

        // Prepare this response
        response.setContentType("text/plain");
        PrintWriter writer = response.getWriter();

        // Forward to the first servlet
        RequestDispatcher rd =
            getServletContext().getRequestDispatcher("/servlet/Forward07a");
        if (rd == null) {
            writer.println("Forward07 FAILED - No request dispatcher" +
                           " for /servlet/Forward07a");
        } else {
            rd.forward(request, response);
            writer.println("Forward07 text should NOT be present");
        }

        // Static logger output (should not actually be rendered)
        while (true) {
            String message = StaticLogger.read();
            if (message == null)
                break;
            writer.println(message);
        }
        StaticLogger.reset();

    }

}
