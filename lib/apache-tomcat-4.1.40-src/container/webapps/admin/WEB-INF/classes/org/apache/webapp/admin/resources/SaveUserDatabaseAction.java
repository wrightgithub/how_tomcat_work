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


package org.apache.webapp.admin.resources;

import java.io.IOException;
import java.util.Locale;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

import org.apache.struts.Globals;
import org.apache.struts.action.Action;
import org.apache.struts.action.ActionForm;
import org.apache.struts.action.ActionForward;
import org.apache.struts.action.ActionMapping;
import javax.management.Attribute;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import org.apache.struts.util.MessageResources;
import org.apache.webapp.admin.ApplicationServlet;


/**
 * <p>Implementation of <strong>Action</strong> that saves a new or
 * updated User database entry.</p>
 *
 * @author Manveen Kaur
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 * @since 4.1
 */

public final class SaveUserDatabaseAction extends Action {


    // ----------------------------------------------------- Instance Variables

    /**
     * The MessageResources we will be retrieving messages from.
     */
    private MessageResources resources = null;


    /**
     * The MBeanServer we will be interacting with.
     */
    private MBeanServer mserver = null;

    /*
     * Only one implementation of factory allowed to start with.
     */
    public static String USERDB_FACTORY = "org.apache.catalina.users.MemoryUserDatabaseFactory";

    // --------------------------------------------------------- Public Methods


    /**
     * Process the specified HTTP request, and create the corresponding HTTP
     * response (or forward to another web component that will create it).
     * Return an <code>ActionForward</code> instance describing where and how
     * control should be forwarded, or <code>null</code> if the response has
     * already been completed.
     *
     * @param mapping The ActionMapping used to select this instance
     * @param actionForm The optional ActionForm bean for this request (if any)
     * @param request The HTTP request we are processing
     * @param response The HTTP response we are creating
     *
     * @exception IOException if an input/output error occurs
     * @exception ServletException if a servlet exception occurs
     */
    public ActionForward execute(ActionMapping mapping,
                                 ActionForm form,
                                 HttpServletRequest request,
                                 HttpServletResponse response)
        throws IOException, ServletException {

        // Look up the components we will be using as needed
        if (mserver == null) {
            mserver = ((ApplicationServlet) getServlet()).getServer();
        }
        if (resources == null) {
            resources = getResources(request);
        }
        HttpSession session = request.getSession();
        Locale locale = (Locale) session.getAttribute(Globals.LOCALE_KEY);

        // Has this transaction been cancelled?
        if (isCancelled(request)) {
            return (mapping.findForward("List UserDatabases Setup"));
        }

        // Check the transaction token
        if (!isTokenValid(request)) {
            response.sendError
                (HttpServletResponse.SC_BAD_REQUEST,
                 resources.getMessage(locale, "users.error.token"));
            return (null);
        }

        // Perform any extra validation that is required
        UserDatabaseForm userDatabaseForm = (UserDatabaseForm) form;
        String objectName = userDatabaseForm.getObjectName();

        // Perform an "Add UserDatabase" transaction
        if (objectName == null) {

            String signature[] = new String[2];
            signature[0] = "java.lang.String";
            signature[1] = "java.lang.String";

            Object params[] = new Object[2];
            params[0] = userDatabaseForm.getName();
            params[1] = ResourceUtils.USERDB_CLASS;     
           
            ObjectName oname = null;

            try {

                // Construct the MBean Name for the naming source
                oname = new ObjectName(ResourceUtils.NAMINGRESOURCES_TYPE + 
                                                    ResourceUtils.GLOBAL_TYPE);

                // Create the new object and associated MBean
                objectName = (String) mserver.invoke(oname, "addResource",
                                                     params, signature);
                                     
            } catch (Exception e) {

                getServlet().log
                    (resources.getMessage(locale, "users.error.invoke",
                                          "addResource"), e);
                response.sendError
                    (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                     resources.getMessage(locale, "users.error.invoke",
                                          "addResource"));
                return (null);
            }

        }
        
        // Perform an "Update User database" transaction
        String attribute = null;
        try {
            
            ObjectName oname = new ObjectName(objectName);

            attribute = "pathname";
            mserver.setAttribute
                (oname,
                 new Attribute(attribute, userDatabaseForm.getPath()));
            attribute = "factory";
            mserver.setAttribute
                (oname,
                 new Attribute(attribute, userDatabaseForm.getFactory()));
            attribute = "description";
            mserver.setAttribute
                (oname,
                 new Attribute(attribute, userDatabaseForm.getDescription()));

        } catch (Exception e) {

            getServlet().log
                (resources.getMessage(locale, "users.error.set.attribute",
                                      attribute), e);
            response.sendError
                (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                 resources.getMessage(locale, "users.error.set.attribute",
                                      attribute));
            return (null);

        }
        
        // Proceed to the list entries screen
        return (mapping.findForward("UserDatabases List Setup"));

    }


}
