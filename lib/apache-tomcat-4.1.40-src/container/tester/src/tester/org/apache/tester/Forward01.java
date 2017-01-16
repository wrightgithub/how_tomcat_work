/*
 * Copyright 1999, 2000 ,2004 The Apache Software Foundation.
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
import java.security.Principal;
import javax.servlet.*;
import javax.servlet.http.*;

/**
 * Positive test for forwarding to a static resource.
 *
 * @author Craig R. McClanahan
 * @version $Revision: 289023 $ $Date: 2004-08-26 23:06:34 +0100 (Thu, 26 Aug 2004) $
 */

public class Forward01 extends HttpServlet {

    public void doGet(HttpServletRequest request, HttpServletResponse response)
        throws IOException, ServletException {

        response.setContentType("text/plain");
        RequestDispatcher rd =
            getServletContext().getRequestDispatcher("/Forward01.txt");
        if (rd == null) {
            PrintWriter writer = response.getWriter();
            writer.println("Forward01 FAILED - No RequestDispatcher returned");
            return;
        }
        rd.forward(request, response);
        StaticLogger.reset();

    }

}
