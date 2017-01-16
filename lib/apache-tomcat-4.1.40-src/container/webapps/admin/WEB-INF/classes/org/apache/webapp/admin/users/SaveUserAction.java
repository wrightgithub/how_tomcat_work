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


package org.apache.webapp.admin.users;


import java.io.IOException;
import java.net.URLDecoder;
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
 * updated User back to the underlying database.</p>
 *
 * @author Craig R. McClanahan
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 * @since 4.1
 */

public final class SaveUserAction extends Action {


    // ----------------------------------------------------- Instance Variables


    /**
     * The MessageResources we will be retrieving messages from.
     */
    private MessageResources resources = null;


    /**
     * The MBeanServer we will be interacting with.
     */
    private MBeanServer mserver = null;


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
            return (mapping.findForward("List Users Setup"));
        }

        // Check the transaction token
        if (!isTokenValid(request)) {
            response.sendError
                (HttpServletResponse.SC_BAD_REQUEST,
                 resources.getMessage(locale, "users.error.token"));
            return (null);
        }

        // Perform any extra validation that is required
        UserForm userForm = (UserForm) form;
        String databaseName =
            URLDecoder.decode(userForm.getDatabaseName());
        String objectName = userForm.getObjectName();

        // Perform an "Add User" transaction
        if (objectName == null) {

            String signature[] = new String[3];
            signature[0] = "java.lang.String";
            signature[1] = "java.lang.String";
            signature[2] = "java.lang.String";

            Object params[] = new Object[3];
            params[0] = userForm.getUsername();
            params[1] = userForm.getPassword();
            params[2] = userForm.getFullName();

            ObjectName oname = null;

            try {

                // Construct the MBean Name for our UserDatabase
                oname = new ObjectName(databaseName);

                // Create the new object and associated MBean
                objectName = (String) mserver.invoke(oname, "createUser",
                                                     params, signature);

            } catch (Exception e) {

                getServlet().log
                    (resources.getMessage(locale, "users.error.invoke",
                                          "createUser"), e);
                response.sendError
                    (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                     resources.getMessage(locale, "users.error.invoke",
                                          "createUser"));
                return (null);
            }

        }

        // Perform an "Update User" transaction
        else {

            ObjectName oname = null;
            String attribute = null;

            try {

                // Construct an object name for this object
                oname = new ObjectName(objectName);

                // Update the specified user
                attribute = "fullName";
                mserver.setAttribute
                    (oname,
                     new Attribute(attribute, userForm.getFullName()));
                attribute = "password";
                mserver.setAttribute
                    (oname,
                     new Attribute(attribute, userForm.getPassword()));

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

        }

        // Reset the groups this user is a member of
        try {

            ObjectName oname = new ObjectName(objectName);
            mserver.invoke(oname, "removeGroups",
                           new Object[0], new String[0]);
            String groups[] = userForm.getGroups();
            if (groups == null) {
                groups = new String[0];
            }
            String addsig[] = new String[1];
            addsig[0] = "java.lang.String";
            Object addpar[] = new Object[1];
            for (int i = 0; i < groups.length; i++) {
                addpar[0] =
                    (new ObjectName(groups[i])).getKeyProperty("groupname");
                mserver.invoke(oname, "addGroup",
                               addpar, addsig);
            }

        } catch (Exception e) {

            getServlet().log
                (resources.getMessage(locale, "users.error.invoke",
                                      "addGroup"), e);
            response.sendError
                (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                 resources.getMessage(locale, "users.error.invoke",
                                      "addGroup"));
            return (null);

        }

        // Reset the roles associated with this user
        try {

            ObjectName oname = new ObjectName(objectName);
            mserver.invoke(oname, "removeRoles",
                           new Object[0], new String[0]);
            String roles[] = userForm.getRoles();
            if (roles == null) {
                roles = new String[0];
            }
            String addsig[] = new String[1];
            addsig[0] = "java.lang.String";
            Object addpar[] = new Object[1];
            for (int i = 0; i < roles.length; i++) {
                addpar[0] =
                    (new ObjectName(roles[i])).getKeyProperty("rolename");
                mserver.invoke(oname, "addRole",
                               addpar, addsig);
            }

        } catch (Exception e) {

            getServlet().log
                (resources.getMessage(locale, "users.error.invoke",
                                      "addRole"), e);
            response.sendError
                (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                 resources.getMessage(locale, "users.error.invoke",
                                      "addRole"));
            return (null);

        }

        // Save the updated database information
        try {

            ObjectName dname = new ObjectName(databaseName);
            mserver.invoke(dname, "save",
                           new Object[0], new String[0]);

        } catch (Exception e) {

            getServlet().log
                (resources.getMessage(locale, "users.error.invoke",
                                      "save"), e);
            response.sendError
                (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                 resources.getMessage(locale, "users.error.invoke",
                                      "save"));
            return (null);

        }

        // Proceed to the list roles screen
        return (mapping.findForward("Users List Setup"));

    }


}
