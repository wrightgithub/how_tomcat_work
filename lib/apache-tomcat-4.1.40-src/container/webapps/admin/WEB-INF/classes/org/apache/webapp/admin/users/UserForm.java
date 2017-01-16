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


import java.net.URLDecoder;
import javax.management.MBeanServer;
import javax.servlet.http.HttpServletRequest;
import org.apache.struts.action.ActionMessage;
import org.apache.struts.action.ActionErrors;
import org.apache.struts.action.ActionMapping;
import org.apache.webapp.admin.ApplicationServlet;


/**
 * Form bean for the individual user page.
 *
 * @author Craig R. McClanahan
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 * @since 4.1
 */

public final class UserForm extends BaseForm {


    // ----------------------------------------------------- Instance Variables

   /**
     * The MBeanServer we will be interacting with.
     */
    private MBeanServer mserver = null;

    // ------------------------------------------------------------- Properties


    /**
     * The full name of the associated user.
     */
    private String fullName = null;

    public String getFullName() {
        return (this.fullName);
    }

    public void setFullName(String fullName) {
        this.fullName = fullName;
    }


    /**
     * The MBean Names of the groups associated with this user.
     */
    private String groups[] = new String[0];

    public String[] getGroups() {
        return (this.groups);
    }

    public void setGroups(String groups[]) {
        if (groups == null) {
            groups = new String[0];
        }
        this.groups = groups;
    }


    /**
     * The password of the associated user.
     */
    private String password = null;

    public String getPassword() {
        return (this.password);
    }

    public void setPassword(String password) {
        this.password = password;
    }


    /**
     * The MBean Names of the roles associated with this user.
     */
    private String roles[] = new String[0];

    public String[] getRoles() {
        return (this.roles);
    }

    public void setRoles(String roles[]) {
        if (roles == null) {
            roles = new String[0];
        }
        this.roles = roles;
    }


    /**
     * The username of the associated user.
     */
    private String username = null;

    public String getUsername() {
        return (this.username);
    }

    public void setUsername(String username) {
        this.username = username;
    }


    // --------------------------------------------------------- Public Methods

    /**
     * Reset all properties to their default values.
     *
     * @param mapping The mapping used to select this instance
     * @param request The servlet request we are processing
     */
    public void reset(ActionMapping mapping, HttpServletRequest request) {

        super.reset(mapping, request);
        fullName = null;
        groups = new String[0];
        password = null;
        roles = new String[0];
        username = null;

    }


    /**
     * Validate the properties that have been set from this HTTP request,
     * and return an <code>ActionErrors</code> object that encapsulates any
     * validation errors that have been found.  If no errors are found, return
     * <code>null</code> or an <code>ActionErrors</code> object with no
     * recorded error messages.
     *
     * @param mapping The mapping used to select this instance
     * @param request The servlet request we are processing
     */
    public ActionErrors validate(ActionMapping mapping,
    HttpServletRequest request) {
        
        try {
            // Look up the components we will be using as needed
            if (mserver == null) {
                mserver = ((ApplicationServlet) getServlet()).getServer();
            }
         
            // Set up beans containing all possible groups and roles
            String databaseName =
                URLDecoder.decode(request.getParameter("databaseName"));
            request.setAttribute("groupsForm",
                                 UserUtils.getGroupsForm(mserver,
                                                         databaseName));
            request.setAttribute("rolesForm",
                                 UserUtils.getRolesForm(mserver,
                                                        databaseName));
        } catch (Exception e) {
            // do nothing since the form returns validation error
        }

        ActionErrors errors = new ActionErrors();

        // username is a required field
        if ((username == null) || (username.length() < 1)) {
            errors.add("username",
                       new ActionMessage("users.error.username.required"));
        }

        // uassword is a required field
        if ((password == null) || (username.length() < 1)) {
            errors.add("password",
                       new ActionMessage("users.error.password.required"));
        }

        // Quotes not allowed in username
        if ((username != null) && (username.indexOf('"') >= 0)) {
            errors.add("username",
                       new ActionMessage("users.error.quotes"));
        }

        // Quotes not allowed in password
        if ((password != null) && (password.indexOf('"') > 0)) {
            errors.add("description",
                       new ActionMessage("users.error.quotes"));
        }

        // Quotes not allowed in fullName
        if ((fullName != null) && (fullName.indexOf('"') > 0)) {
            errors.add("fullName",
                       new ActionMessage("users.error.quotes"));
        }

        return (errors);

    }


}
