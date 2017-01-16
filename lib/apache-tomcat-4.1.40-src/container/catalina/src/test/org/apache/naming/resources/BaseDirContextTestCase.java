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


package org.apache.naming.resources;

import java.util.Date;

import javax.naming.Binding;
import javax.naming.NameClassPair;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;

import javax.naming.directory.Attribute;
import javax.naming.directory.Attributes;
import javax.naming.directory.DirContext;

import junit.framework.TestCase;


/**
 * <p>Basic unit tests for the <code>javax.naming.directory.DirContext</code>
 * implementations.  This class must be subclassed for each individual
 * <code>DirContext</code> implementation.</p>
 *
 * <p><strong>WARNING</strong>:  These tests make certain assumptions that
 * can generate "false negative" results if they are violated:</p>
 * <ul>
 * <li>The pathname of a directory (or WAR file) containing the static
 *     resources of the <code>/examples</code> web application shipped
 *     with Tomcat is passed to our test class as a system property
 *     named <code>doc.base</code>.</li>
 * <li>The entry names that can be found in the top-level DirContext of
 *     the static resources are enumerated in the <code>topLevelNames</code>
 *     variable.</li>
 * <li>The entry names that can be found in the WEB-INF DirContext of
 *     the static resources are enumerated in the <code>webInfNames</code>
 *     variable.</li>
 * <li>The entry names in either the top-level or WEB-INF DirContext contexts
 *     that should themselves be <code>DirContext</code> implementations (i.e.
 *     "subdirectories" in the static resources for this web application)
 *     are enumerated in the <code>dirContextNames</code> variable.</li>
 * </ul>
 *
 * @author Craig R. McClanahan
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 */

public abstract class BaseDirContextTestCase extends TestCase {


    // ----------------------------------------------------- Instance Variables


    /**
     * A directory context implementation to be tested.
     */
    protected DirContext context = null;


    /**
     * The pathname of the document base directory for this directory context.
     */
    protected String docBase = System.getProperty("doc.base");


    /**
     * Entry names that must be DirContexts.  Names not on this list are
     * assumed to be Resources.
     */
    protected static final String dirContextNames[] =
    { "classes", "images", "jsp", "lib", "META-INF", "WEB-INF" };


    /**
     * The set of names that should be present in the top-level
     * directory context.
     */
    protected static final String topLevelNames[] =
    { "images", "jsp", "servlets", "META-INF", "WEB-INF" };


    /**
     * The set of names that should be present in the WEB-INF
     * directory context.
     */
    protected static final String webInfNames[] =
    { "classes", "jsp", "lib", "web.xml" };


    /**
     * The set of names that should be attributes of WEB-INF.
     */
    protected static final String webInfAttrs[] =
    { "creationdate", "displayname", "getcontentlength", "getlastmodified",
      "resourcetype" };


    /**
     * The set of names that should be attributes of WEB-INF/web.xml.
     */
    protected static final String webXmlAttrs[] =
    { "creationdate", "displayname", "getcontentlength", "getlastmodified",
      "resourcetype" };


    // ----------------------------------------------------------- Constructors


    /**
     * Construct a new instance of this test case.
     *
     * @param name Name of the test case
     */
    public BaseDirContextTestCase(String name) {

        super(name);

    }


    // --------------------------------------------------- Overall Test Methods


    /**
     * Set up instance variables required by this test case.  This method
     * <strong>MUST</strong> be implemented by a subclass.
     */
    public abstract void setUp();


    /**
     * Return the tests included in this test suite.  This method
     * <strong>MUST</strong> be implemented by a subclass.
     */
    // public abstract static Test suite();


    /**
     * Tear down instance variables required by this test case.  This method
     * <strong>MUST</strong> be implemented by a subclass.
     */
    public abstract void tearDown();


    // ------------------------------------------------ Individual Test Methods


    /**
     * Test the attributes returned for the <code>WEB-INF</code> entry.
     */
    public abstract void testGetAttributesWebInf();


    /**
     * Test the attributes returned for the <code>WEB-INF/web.xml</code>
     * entry.
     */
    public abstract void testGetAttributesWebXml();


    /**
     * We should be able to list the contents of the top-level context itself,
     * and locate some entries we know are there.
     */
    public void testListTopLevel() {

        try {
            checkList(context.list(""), topLevelNames);
        } catch (NamingException e) {
            fail("NamingException: " + e);
        }

    }


    /**
     * We should be able to list the contents of the WEB-INF entry,
     * and locate some entries we know are there.
     */
    public void testListWebInfDirect() {

        try {

            // Look up the WEB-INF entry
            Object webInfEntry = context.lookup("WEB-INF");
            assertNotNull("Found WEB-INF entry", webInfEntry);
            assertTrue("WEB-INF entry is a DirContext",
                       webInfEntry instanceof DirContext);
            DirContext webInfContext = (DirContext) webInfEntry;

            // Check the contents of the WEB-INF context directly
            checkList(webInfContext.list(""), webInfNames);

        } catch (NamingException e) {
            fail("NamingException: " + e);
        }


    }


    /**
     * We should be able to list the contents of the WEB-INF entry,
     * and locate some entries we know are there.
     */
    public void testListWebInfIndirect() {

        try {
            checkList(context.list("WEB-INF"), webInfNames);
        } catch (NamingException e) {
            fail("NamingException: " + e);
        }

    }


    /**
     * We should be able to list the bindings of the top-level context itself,
     * and locate some entries we know are there.
     */
    public void testListBindingsTopLevel() {

        try {
            checkListBindings(context.listBindings(""), topLevelNames);
        } catch (NamingException e) {
            fail("NamingException: " + e);
        }

    }


    /**
     * We should be able to list the bindings of the WEB-INF entry,
     * and locate some entries we know are there.
     */
    public void testListBindingsWebInfDirect() {

        try {

            // Look up the WEB-INF entry
            Object webInfEntry = context.lookup("WEB-INF");
            assertNotNull("Found WEB-INF entry", webInfEntry);
            assertTrue("WEB-INF entry is a DirContext",
                       webInfEntry instanceof DirContext);
            DirContext webInfContext = (DirContext) webInfEntry;

            // Check the bindings of the WEB-INF context directly
            checkListBindings(webInfContext.listBindings(""), webInfNames);

        } catch (NamingException e) {
            fail("NamingException: " + e);
        }


    }


    /**
     * We should be able to list the bindings of the WEB-INF entry,
     * and locate some entries we know are there.
     */
    public void testListBindingsWebInfIndirect() {

        try {
            checkListBindings(context.listBindings("WEB-INF"), webInfNames);
        } catch (NamingException e) {
            fail("NamingException: " + e);
        }

    }


    // -------------------------------------------------------- Support Methods


    /**
     * Check the results of a list() call against the specified entry list.
     *
     * @param enum The naming enumeration we are checking
     * @param list The list of entry names we are expecting
     *
     * @exception NamingException if a naming exception occurs
     */
    protected void checkList(NamingEnumeration ne, String list[])
        throws NamingException {

        String contextClassName = context.getClass().getName();

        assertNotNull("NamingEnumeration is not null", ne);
        while (ne.hasMore()) {

            Object next = ne.next();
            assertTrue("list() returns NameClassPair instances",
                       next instanceof NameClassPair);
            NameClassPair ncp = (NameClassPair) next;

            assertTrue("Name '" + ncp.getName() + "' is expected",
                       isListed(ncp.getName(), list));

            if (isDirContext(ncp.getName())) {
                assertTrue("Class '" + ncp.getClassName() + "' is '" +
                           contextClassName + "'",
                           contextClassName.equals(ncp.getClassName()));
            }

            assertTrue("Relative is 'true'", ncp.isRelative());

        }

    }


    /**
     * Check the results of a listBindings() call against the
     * specified entry list.
     *
     * @param enum The naming enumeration we are checking
     * @param list The list of entry names we are expecting
     *
     * @exception NamingException if a naming exception occurs
     */
    protected void checkListBindings(NamingEnumeration ne, String list[])
        throws NamingException {

        String contextClassName = context.getClass().getName();

        assertNotNull("NamingEnumeration is not null", ne);
        while (ne.hasMore()) {

            Object next = ne.next();
            assertTrue("listBindings() returns Binding instances",
                       next instanceof Binding);
            Binding b = (Binding) next;

            assertTrue("Name '" + b.getName() + "' is expected",
                       isListed(b.getName(), list));

            if (isDirContext(b.getName())) {
                assertTrue("Class '" + b.getClassName() + "' is '" +
                           contextClassName + "'",
                           contextClassName.equals(b.getClassName()));
            }

            assertTrue("Relative is 'true'", b.isRelative());

            Object object = b.getObject();
            assertNotNull("Name '" + b.getName() + "' has a non-null object",
                          object);
            if (b.getName().equals("web.xml")) {
                assertTrue("Entry '" + b.getName() + "' is a Resource",
                           object instanceof Resource);
            } else {
                assertTrue("Entry '" + b.getName() + "' is a DirContext",
                           object instanceof DirContext);
            }

        }

    }


    /**
     * Check the attributes associated with a WEB-INF entry.
     *
     * @param attrs The attributes for this entry
     * @param creationDate The creation date for this entry
     * @param contentLength The content length for this entry
     * @param displayName The display name for this entry
     * @param lastModifiedDate The last modified date for this entry
     */
    protected void checkWebInfAttributes(Attributes attrs,
                                         Date creationDate,
                                         long contentLength,
                                         String displayName,
                                         Date lastModifiedDate)
        throws NamingException {

        assertNotNull("getAttributes() returned non-null", attrs);

        NamingEnumeration ne = attrs.getAll();
        assertNotNull("getAll() returned non-null", ne);
        while (ne.hasMore()) {

            Object next = ne.next();
            assertTrue("getAll() returns Attribute instances",
                       next instanceof Attribute);
            Attribute attr = (Attribute) next;
            String name = attr.getID();
            int index = getIndex(name, webInfAttrs);
            assertTrue("WEB-INF attribute '" + name + "' is expected",
                       index >= 0);
            Object value = attr.get();
            assertNotNull("get() returned non-null", value);

            if (name.equals("creationdate")) {
                assertTrue("Creation date is a date",
                           value instanceof Date);
                assertTrue("Creation date equals " + creationDate,
                           creationDate.equals((Date) value));
            } else if (name.equals("displayname")) {
                assertTrue("Display name is a string",
                           value instanceof String);
                assertTrue("Display name equals " + displayName,
                           displayName.equals((String) value));
            } else if (name.equals("getcontentlength")) {
                assertTrue("Content length is a long",
                           value instanceof Long);
                assertTrue("Content length equals " + contentLength,
                           contentLength == ((Long) value).longValue());
            } else if (name.equals("getlastmodified")) {
                assertTrue("Last modified date is a date",
                           value instanceof Date);
                assertTrue("Last modified date is " + lastModifiedDate,
                           lastModifiedDate.equals((Date) value));
            }

        }

    }


    /**
     * Check the attributes associated with a WEB-INF/web.xml entry.
     *
     * @param attrs The attributes for this entry
     * @param creationDate The creation date for this entry
     * @param contentLength The content length for this entry
     * @param displayName The display name for this entry
     * @param lastModifiedDate The last modified date for this entry
     */
    protected void checkWebXmlAttributes(Attributes attrs,
                                         Date creationDate,
                                         long contentLength,
                                         String displayName,
                                         Date lastModifiedDate)
        throws NamingException {

        assertNotNull("getAttributes() returned non-null", attrs);

        NamingEnumeration ne = attrs.getAll();
        assertNotNull("getAll() returned non-null", ne);
        while (ne.hasMore()) {

            Object next = ne.next();
            assertTrue("getAll() returns Attribute instances",
                       next instanceof Attribute);
            Attribute attr = (Attribute) next;
            String name = attr.getID();
            int index = getIndex(name, webXmlAttrs);
            assertTrue("WEB-INF/web.xml attribute '" + name + "' is expected",
                       index >= 0);
            Object value = attr.get();
            assertNotNull("get() returned non-null", value);

            if (name.equals("creationdate")) {
                assertTrue("Creation date is a date",
                           value instanceof Date);
                assertTrue("Creation date equals " + creationDate,
                           creationDate.equals((Date) value));
            } else if (name.equals("displayname")) {
                assertTrue("Display name is a string",
                           value instanceof String);
                assertTrue("Display name equals " + displayName,
                           displayName.equals((String) value));
            } else if (name.equals("getcontentlength")) {
                assertTrue("Content length is a long",
                           value instanceof Long);
                assertTrue("Content length equals " + contentLength,
                           contentLength == ((Long) value).longValue());
            } else if (name.equals("getlastmodified")) {
                assertTrue("Last modified date is a date",
                           value instanceof Date);
                assertTrue("Last modified date is " + lastModifiedDate,
                           lastModifiedDate.equals((Date) value));
            }

        }

    }


    /**
     * Return the index of the specified name in the specified list, or
     * -1 if the name is not present.
     *
     * @param name Name to be looked up
     * @param list List of names to be checked
     */
    protected int getIndex(String name, String list[]) {

        for (int i = 0; i < list.length; i++) {
            if (name.equals(list[i]))
                return (i);
        }
        return (-1);

    }


    /**
     * Is an entry of the specified name supposed to be a DirContext?
     *
     * @param name Name to be checked
     */
    protected boolean isDirContext(String name) {

        return (isListed(name, dirContextNames));

    }


    /**
     * Returns <code>true</code> if the specified name is found in the
     * specified list.
     *
     * @param name Name to be looked up
     * @param list List to be looked up into
     */
    protected boolean isListed(String name, String list[]) {

        return (getIndex(name, list) >= 0);

    }


}
