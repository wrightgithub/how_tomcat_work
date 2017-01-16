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

import javax.servlet.http.HttpServletRequest;
import org.apache.struts.action.ActionMessage;
import org.apache.struts.action.ActionErrors;
import org.apache.struts.action.ActionMapping;

/**
 * Form bean for the individual mail session page.
 *
 * @author Amy Roh
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 * @since 4.1
 */

public final class MailSessionForm extends BaseForm {


    // ----------------------------------------------------- Instance Variables


    // ------------------------------------------------------------- Properties

    
    /**
     * The name of the mail session.
     */
    private String name = null;

    public String getName() {
        return (this.name);
    }

    public void setName(String name) {
        this.name = name;
    }
    
    /**
     * The mail.smtp.host of the mail session.
     */
    private String mailhost = null;

    public String getMailhost() {
        return (this.mailhost);
    }

    public void setMailhost(String mailhost) {
        this.mailhost = mailhost;
    }

    /**
     * The resource type of this mail session.
     */
    private String resourcetype = null;
    
    /**
     * Return the resource type of the mail session this bean refers to.
     */
    public String getResourcetype() {
        return this.resourcetype;
    }

    /**
     * Set the resource type of the mail session this bean refers to.
     */
    public void setResourcetype(String resourcetype) {
        this.resourcetype = resourcetype;
    }
       
    /**
     * The path of this mail session.
     */
    private String path = null;
    
    /**
     * Return the path of the mail session this bean refers to.
     */
    public String getPath() {
        return this.path;
    }

    /**
     * Set the path of the mail session this bean refers to.
     */
    public void setPath(String path) {
        this.path = path;
    }
       
    /**
     * The host of this mail session.
     */
    private String host = null;
    
    /**
     * Return the host of the mail session this bean refers to.
     */
    public String getHost() {
        return this.host;
    }

    /**
     * Set the host of the mail session this bean refers to.
     */
    public void setHost(String host) {
        this.host = host;
    }    
    
       
    /**
     * The service of this mail session.
     */
    private String service = null;
    
    /**
     * Return the service of the mail session this bean refers to.
     */
    public String getService() {
        return this.service;
    }

    /**
     * Set the service of the mail session this bean refers to.
     */
    public void setService(String service) {
        this.service = service;
    }
    
    /**
     * The type of the resource.
     */
    private String type = null;

    public String getType() {
        return (this.type);
    }

    public void setType(String type) {
        this.type = type;
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
        mailhost = null;   
        type = null;
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
    
    private ActionErrors errors = null;
    
    public ActionErrors validate(ActionMapping mapping,
    HttpServletRequest request) {

        errors = new ActionErrors();

        // mailSmtpHost is a required field
        if ((mailhost == null) || (mailhost.length() < 1)) {
            errors.add("mailhost",
                  new ActionMessage("resources.error.mailhost.required"));
        }
        
        return (errors);
    }
    
}
