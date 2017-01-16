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
import org.apache.struts.action.ActionMapping;


/**
 * Form bean for the delete env entries page.
 *
 * @author Manveen Kaur
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 * @since 4.1
 */

public final class EnvEntriesForm extends BaseForm {


    // ----------------------------------------------------- Instance Variables


    // ------------------------------------------------------------- Properties


    /**
     * The object names of the specified environment entries.
     */
    private String envEntries[] = null;

    public String[] getEnvEntries() {
        return (this.envEntries);
    }

    public void setEnvEntries(String envEntries[]) {
        this.envEntries = envEntries;
    }
    
    /**
     * The resource type of this environment entry.
     */
    private String resourcetype = null;
    
    /**
     * Return the resource type of the environment entry this bean refers to.
     */
    public String getResourcetype() {
        return this.resourcetype;
    }

    /**
     * Set the resource type of the environment entry this bean refers to.
     */
    public void setResourcetype(String resourcetype) {
        this.resourcetype = resourcetype;
    }
       
    /**
     * The path of this environment entry.
     */
    private String path = null;
    
    /**
     * Return the path of the environment entry this bean refers to.
     */
    public String getPath() {
        return this.path;
    }

    /**
     * Set the path of the environment entry this bean refers to.
     */
    public void setPath(String path) {
        this.path = path;
    }
       
    /**
     * The host of this environment entry.
     */
    private String host = null;
    
    /**
     * Return the host of the environment entry this bean refers to.
     */
    public String getHost() {
        return this.host;
    }

    /**
     * Set the host of the environment entry this bean refers to.
     */
    public void setHost(String host) {
        this.host = host;
    }    
    
       
    /**
     * The service of this environment entry.
     */
    private String service = null;
    
    /**
     * Return the service of the environment entry this bean refers to.
     */
    public String getService() {
        return this.service;
    }

    /**
     * Set the service of the environment entry this bean refers to.
     */
    public void setService(String service) {
        this.service = service;
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
        this.envEntries = null;

    }


}
