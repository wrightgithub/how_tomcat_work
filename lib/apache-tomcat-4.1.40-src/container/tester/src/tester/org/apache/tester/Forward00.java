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
 * Exercise basic forwarding functionality.
 *
 * @author Craig R. McClanahan
 * @version $Revision: 289023 $ $Date: 2004-08-26 23:06:34 +0100 (Thu, 26 Aug 2004) $
 */

public class Forward00 extends HttpServlet {


    public void doGet(HttpServletRequest request, HttpServletResponse response)
        throws IOException, ServletException {

        // Prepare this response
        StringBuffer sb = new StringBuffer();
        response.setContentType("text/plain");
	PrintWriter writer = response.getWriter();

        // Acquire the path to which we will issue a forward
        String path = request.getParameter("path");
        if (path == null)
            path = "/Forward00a";

        // Create a request dispatcher and call forward() on it
        RequestDispatcher rd = null;
        if (path.startsWith("!"))
            rd = getServletContext().getNamedDispatcher(path.substring(1));
        else
            rd = getServletContext().getRequestDispatcher(path);
        if (rd == null) {
            sb.append(" No RequestDispatcher returned/");
        } else {
            if (sb.length() < 1)
                rd.forward(request, response);
        }

        // Write our response if an error occurred
        if (sb.length() >= 1) {
            writer.print("Forward00 FAILED -");
            writer.println(sb.toString());
            while (true) {
                String message = StaticLogger.read();
                if (message == null)
                    break;
                writer.println(message);
            }
        }
        StaticLogger.reset();

    }

}
