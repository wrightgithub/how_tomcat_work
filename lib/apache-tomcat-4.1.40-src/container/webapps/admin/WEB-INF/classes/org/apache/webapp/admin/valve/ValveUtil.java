/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.webapp.admin.valve;

import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Locale;
import java.io.IOException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import org.apache.regexp.RE;
import org.apache.regexp.RESyntaxException;
import org.apache.struts.Globals;
import org.apache.struts.action.ActionMapping;
import org.apache.struts.util.MessageResources;
import org.apache.webapp.admin.ApplicationServlet;
import org.apache.webapp.admin.TomcatTreeBuilder;
import org.apache.webapp.admin.TreeControl;
import org.apache.webapp.admin.TreeControlNode;

/**
 * A utility class that contains methods common across valves.
 *
 * @author Manveen Kaur
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 */

public final class ValveUtil {
    
    
    // ----------------------------------------------------- Instance Variables
    
    /**
     * Signature for the <code>createStandardValve</code> operation.
     */
    private static String createStandardValveTypes[] =
    { "java.lang.String",     // parent
    };
    
    
    // --------------------------------------------------------- Public Methods
    
    public static String createValve(String parent, String valveType,
    HttpServletResponse response, HttpServletRequest request,
    ActionMapping mapping, ApplicationServlet servlet)
    throws IOException, ServletException {
        
        MessageResources resources = (MessageResources)
            servlet.getServletContext().getAttribute(Globals.MESSAGES_KEY);
        HttpSession session = request.getSession();
        
        MBeanServer mBServer = null;
        Locale locale = (Locale) session.getAttribute(Globals.LOCALE_KEY);
        // Acquire a reference to the MBeanServer containing our MBeans
        try {
            mBServer = servlet.getServer();
        } catch (Throwable t) {
            throw new ServletException
            ("Cannot acquire MBeanServer reference", t);
        }
        
        String operation = null;
        String values[] = null;
        String vObjectName = null;
        
        try {
            
            String parentNodeName = parent;
            ObjectName pname = new ObjectName(parent);
            StringBuffer sb = new StringBuffer(pname.getDomain());
            
            // For service, create the corresponding Engine mBean
            // Parent in this case needs to be the container mBean for the service
            try {
                if ("Service".equalsIgnoreCase(pname.getKeyProperty("type"))) {
                    sb.append(":type=Engine,service=");
                    sb.append(pname.getKeyProperty("name"));
                    parent = sb.toString();
                }
            } catch (Exception e) {
                String message =
                resources.getMessage("error.engineName.bad",
                sb.toString());
                servlet.log(message);
                response.sendError(HttpServletResponse.SC_BAD_REQUEST, message);
                return (null);
            }    
            // Ensure that the requested valve name is unique
            
            // TBD -- do we need this check?
            /*
            ObjectName oname =
            new ObjectName(objectName);
            if (mBServer.isRegistered(oname)) {
                ActionErrors errors = new ActionErrors();
                errors.add("valveName",
                    new ActionMessage("error.valveName.exists"));
                String message =
                    resources.getMessage("error.valveName.exists", sb.toString());
                response.sendError(HttpServletResponse.SC_BAD_REQUEST, message);                
                return (new ActionForward(mapping.getInput()));
            }
            */
            
            // Look up our MBeanFactory MBean
            ObjectName fname =
            new ObjectName(TomcatTreeBuilder.FACTORY_TYPE);
            
            // Create a new StandardValve object
            values = new String[1];            
            values[0] = parent;           
            
            operation = "create" + valveType;
            if ("AccessLogValve".equalsIgnoreCase(valveType))
                operation = "createAccessLoggerValve";
                
            vObjectName = (String)
                        mBServer.invoke(fname, operation, values, createStandardValveTypes);
            
            // Add the new Valve to our tree control node
            TreeControl control = (TreeControl)
            session.getAttribute("treeControlTest");
            if (control != null) {
                TreeControlNode parentNode = control.findNode(parentNodeName);
                if (parentNode != null) {
                    String nodeLabel =
                    "Valve for " + parentNode.getLabel();
                    String encodedName =
                    URLEncoder.encode(vObjectName);
                    TreeControlNode childNode =
                    new TreeControlNode(vObjectName,
                    "Valve.gif",
                    nodeLabel,
                    "EditValve.do?select=" + encodedName +
                    "&nodeLabel=" + URLEncoder.encode(nodeLabel) +
                    "&parent=" + URLEncoder.encode(parentNodeName),
                    "content",
                    true);
                    parentNode.addChild(childNode);
                    // FIXME - force a redisplay
                } else {
                    servlet.log
                    ("Cannot find parent node '" + parentNodeName + "'");
                }
            } else {
                servlet.log
                ("Cannot find TreeControlNode!");
            }
            
        } catch (Exception e) {
            
            servlet.log
            (resources.getMessage(locale, "users.error.invoke",
            operation), e);
            response.sendError
            (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
            resources.getMessage(locale, "users.error.invoke",
            operation));
            return (null);
            
        }
        
        // Forward to the success reporting page
        session.removeAttribute(mapping.getAttribute());
        return vObjectName;
    }

    
    /**
     * Return an array of regular expression objects initialized from the
     * specified argument, which must be <code>null</code> or a comma-delimited
     * list of regular expression patterns.
     *
     * @param list The comma-separated list of patterns
     *
     * @exception IllegalArgumentException if one of the patterns has
     *  invalid syntax
     */
    public static RE[] precalculate(String list) 
                                    throws IllegalArgumentException {

        if (list == null)
            return (new RE[0]);
        list = list.trim();
        if (list.length() < 1)
            return (new RE[0]);
        list += ",";

        ArrayList reList = new ArrayList();
        while (list.length() > 0) {
            int comma = list.indexOf(',');
            if (comma < 0)
                break;
            String pattern = list.substring(0, comma).trim();
            try {
                reList.add(new RE(pattern));
            } catch (RESyntaxException e) {
                throw new IllegalArgumentException
                    ("Syntax error in request filter pattern");
            }
            list = list.substring(comma + 1);
        }

        RE reArray[] = new RE[reList.size()];
        return ((RE[]) reList.toArray(reArray));

    }    

}
