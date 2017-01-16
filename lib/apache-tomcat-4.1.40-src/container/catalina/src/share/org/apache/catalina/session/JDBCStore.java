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

package org.apache.catalina.session;

import java.io.InputStream;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.ConcurrentModificationException;

import org.apache.catalina.Container;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.Loader;
import org.apache.catalina.Session;
import org.apache.catalina.Store;
import org.apache.catalina.util.CustomObjectInputStream;

/**
 * Implementation of the <code>Store</code> interface that stores
 * serialized session objects in a database.  Sessions that are
 * saved are still subject to being expired based on inactivity.
 *
 * @author Bip Thelin
 * @version $Revision: 554107 $, $Date: 2007-07-07 02:37:21 +0100 (Sat, 07 Jul 2007) $
 */

public class JDBCStore
    extends StoreBase implements Store {

    /**
     * The descriptive information about this implementation.
     */
    protected static String info = "JDBCStore/1.0";

    /**
     * Context name associated with this Store
     */
    private String name = null;

    /**
     * Name to register for this Store, used for logging.
     */
    protected static String storeName = "JDBCStore";

    /**
     * Name to register for the background thread.
     */
    protected String threadName = "JDBCStore";

    /**
     * Connection string to use when connecting to the DB.
     */
    protected String connString = null;

    /**
     * The database connection.
     */
    private Connection conn = null;

    /**
     * Driver to use.
     */
    protected String driverName = null;

    // ------------------------------------------------------------- Table & cols

    /**
     * Table to use.
     */
    protected String sessionTable = "tomcat$sessions";

    /**
     * Column to use for /Engine/Host/Context name
     */
    protected String sessionAppCol = "app";

    /**
     * Id column to use.
     */
    protected String sessionIdCol = "id";

    /**
     * Data column to use.
     */
    protected String sessionDataCol = "data";

    /**
     * Is Valid column to use.
     */
    protected String sessionValidCol = "valid";

    /**
     * Max Inactive column to use.
     */
    protected String sessionMaxInactiveCol = "maxinactive";

    /**
     * Last Accessed column to use.
     */
    protected String sessionLastAccessedCol = "lastaccess";

    // ------------------------------------------------------------- SQL Variables

    /**
     * Variable to hold the <code>getSize()</code> prepared statement.
     */
    protected PreparedStatement preparedSizeSql = null;

    /**
     * Variable to hold the <code>keys()</code> prepared statement.
     */
    protected PreparedStatement preparedKeysSql = null;

    /**
     * Variable to hold the <code>save()</code> prepared statement.
     */
    protected PreparedStatement preparedSaveSql = null;

    /**
     * Variable to hold the <code>clear()</code> prepared statement.
     */
    protected PreparedStatement preparedClearSql = null;

    /**
     * Variable to hold the <code>remove()</code> prepared statement.
     */
    protected PreparedStatement preparedRemoveSql = null;

    /**
     * Variable to hold the <code>load()</code> prepared statement.
     */
    protected PreparedStatement preparedLoadSql = null;

    /**
     * Variable to hold the <code>processExpires()</code> prepared statement.
     */
    protected PreparedStatement preparedExpiresSql = null;

    // ------------------------------------------------------------- Properties

    /**
     * Return the info for this Store.
     */
    public String getInfo() {
        return(info);
    }

    /**
     * Return the name for this instance (built from container name)
     */
    public String getName() {
        if (name == null) {
            Container container = manager.getContainer();
            String contextName = container.getName();
            String hostName = "";
            String engineName = "";

            if (container.getParent() != null) {
                Container host = container.getParent();
                hostName = host.getName();
                if (host.getParent() != null) {
                    engineName = host.getParent().getName();
                }
            }
            name = "/" + engineName + "/" + hostName + contextName;
        }
        return name;
    }

    /**
     * Return the thread name for this Store.
     */
    public String getThreadName() {
        return(threadName);
    }

    /**
     * Return the name for this Store, used for logging.
     */
    public String getStoreName() {
        return(storeName);
    }

    /**
     * Set the driver for this Store.
     *
     * @param driverName The new driver
     */
    public void setDriverName(String driverName) {
        String oldDriverName = this.driverName;
        this.driverName = driverName;
        support.firePropertyChange("driverName",
                                   oldDriverName,
                                   this.driverName);
        this.driverName = driverName;
    }

    /**
     * Return the driver for this Store.
     */
    public String getDriverName() {
        return(this.driverName);
    }

    /**
     * Set the Connection URL for this Store.
     *
     * @param connectionURL The new Connection URL
     */
    public void setConnectionURL(String connectionURL) {
        String oldConnString = this.connString;
        this.connString = connectionURL;
        support.firePropertyChange("connString",
                                   oldConnString,
                                   this.connString);
    }

    /**
     * Return the Connection URL for this Store.
     */
    public String getConnectionURL() {
        return(this.connString);
    }

    /**
     * Set the table for this Store.
     *
     * @param sessionTable The new table
     */
    public void setSessionTable(String sessionTable) {
        String oldSessionTable = this.sessionTable;
        this.sessionTable = sessionTable;
        support.firePropertyChange("sessionTable",
                                   oldSessionTable,
                                   this.sessionTable);
    }

    /**
     * Return the table for this Store.
     */
    public String getSessionTable() {
        return(this.sessionTable);
    }

    /**
     * Set the App column for the table.
     *
     * @param sessionAppCol the column name
     */
    public void setSessionAppCol(String sessionAppCol) {
        String oldSessionAppCol = this.sessionAppCol;
        this.sessionAppCol = sessionAppCol;
        support.firePropertyChange("sessionAppCol",
                                   oldSessionAppCol,
                                   this.sessionAppCol);
    }

    /**
     * Return the Id column for the table.
     */
    public String getSessionAppCol() {
        return(this.sessionAppCol);
    }

    /**
     * Set the Id column for the table.
     *
     * @param sessionIdCol the column name
     */
    public void setSessionIdCol(String sessionIdCol) {
        String oldSessionIdCol = this.sessionIdCol;
        this.sessionIdCol = sessionIdCol;
        support.firePropertyChange("sessionIdCol",
                                   oldSessionIdCol,
                                   this.sessionIdCol);
    }

    /**
     * Return the Id column for the table.
     */
    public String getSessionIdCol() {
        return(this.sessionIdCol);
    }

    /**
     * Set the Data column for the table
     *
     * @param sessionDataCol the column name
     */
    public void setSessionDataCol(String sessionDataCol) {
        String oldSessionDataCol = this.sessionDataCol;
        this.sessionDataCol = sessionDataCol;
        support.firePropertyChange("sessionDataCol",
                                   oldSessionDataCol,
                                   this.sessionDataCol);
    }

    /**
     * Return the data column for the table
     */
    public String getSessionDataCol() {
        return(this.sessionDataCol);
    }

    /**
     * Set the Is Valid column for the table
     *
     * @param sessionValidCol The column name
     */
    public void setSessionValidCol(String sessionValidCol) {
        String oldSessionValidCol = this.sessionValidCol;
        this.sessionValidCol = sessionValidCol;
        support.firePropertyChange("sessionValidCol",
                                   oldSessionValidCol,
                                   this.sessionValidCol);
    }

    /**
     * Return the Is Valid column
     */
    public String getSessionValidCol() {
        return(this.sessionValidCol);
    }

    /**
     * Set the Max Inactive column for the table
     *
     * @param sessionMaxInactiveCol The column name
     */
    public void setSessionMaxInactiveCol(String sessionMaxInactiveCol) {
        String oldSessionMaxInactiveCol = this.sessionMaxInactiveCol;
        this.sessionMaxInactiveCol = sessionMaxInactiveCol;
        support.firePropertyChange("sessionMaxInactiveCol",
                                   oldSessionMaxInactiveCol,
                                   this.sessionMaxInactiveCol);
    }

    /**
     * Return the Max Inactive column
     */
    public String getSessionMaxInactiveCol() {
        return(this.sessionMaxInactiveCol);
    }

    /**
     * Set the Last Accessed column for the table
     *
     * @param sessionLastAccessedCol The column name
     */
    public void setSessionLastAccessedCol(String sessionLastAccessedCol) {
        String oldSessionLastAccessedCol = this.sessionLastAccessedCol;
        this.sessionLastAccessedCol = sessionLastAccessedCol;
        support.firePropertyChange("sessionLastAccessedCol",
                                   oldSessionLastAccessedCol,
                                   this.sessionLastAccessedCol);
    }

    /**
     * Return the Last Accessed column
     */
    public String getSessionLastAccessedCol() {
        return(this.sessionLastAccessedCol);
    }

    // --------------------------------------------------------- Public Methods

    /**
     * Return an array containing the session identifiers of all Sessions
     * currently saved in this Store.  If there are no such Sessions, a
     * zero-length array is returned.
     *
     * @exception IOException if an input/output error occurred
     */
    public String[] keys() throws IOException {
        String keysSql =
            "SELECT " + sessionIdCol + " FROM " + sessionTable +
            " WHERE " + sessionAppCol + " = ?";
        ResultSet rst = null;
        String keys[] = null;
        int i;

        synchronized(this) {
            Connection _conn = getConnection();

            if(_conn == null) {
                return(new String[0]);
            }

            try {
                if(preparedKeysSql == null) {
                    preparedKeysSql = _conn.prepareStatement(keysSql);
                }

                preparedKeysSql.setString(1, getName());
                rst = preparedKeysSql.executeQuery();
                ArrayList tmpkeys = new ArrayList();
                if (rst != null) {
                    while(rst.next()) {
                        tmpkeys.add(rst.getString(1));
                    }
                }
                keys = (String[]) tmpkeys.toArray(new String[tmpkeys.size()]);
            } catch(SQLException e) {
                log(sm.getString(getStoreName()+".SQLException", e));
                keys = new String[0];
            } finally {
                try {
                    if(rst != null) {
                        rst.close();
                    }
                } catch(SQLException e) {
                    ;
                }

                release(_conn);
            }
        }
        
        return(keys);
    }

    /**
     * Return an integer containing a count of all Sessions
     * currently saved in this Store.  If there are no Sessions,
     * <code>0</code> is returned.
     *
     * @exception IOException if an input/output error occurred
     */
    public int getSize() throws IOException {
        int size = 0;
        String sizeSql = 
            "SELECT COUNT(" + sessionIdCol + ") FROM " + sessionTable +
            " WHERE " + sessionAppCol + " = ?";
        ResultSet rst = null;

        synchronized(this) {
            Connection _conn = getConnection();

            if(_conn == null) {
                return(size);
            }

            try {
                if(preparedSizeSql == null) {
                    preparedSizeSql = _conn.prepareStatement(sizeSql);
                }

                preparedSizeSql.setString(1, getName());
                rst = preparedSizeSql.executeQuery();
                if (rst.next()) {
                    size = rst.getInt(1);
                }
            } catch(SQLException e) {
                log(sm.getString(getStoreName()+".SQLException", e));
            } finally {
                try {
                    if(rst != null)
                        rst.close();
                } catch(SQLException e) {
                    ;
                }

                release(_conn);
            }
        }
        return(size);
    }

    /**
     * Load the Session associated with the id <code>id</code>.
     * If no such session is found <code>null</code> is returned.
     *
     * @param id a value of type <code>String</code>
     * @return the stored <code>Session</code>
     * @exception ClassNotFoundException if an error occurs
     * @exception IOException if an input/output error occurred
     */
    public Session load(String id)
        throws ClassNotFoundException, IOException {
        ResultSet rst = null;
        StandardSession _session = null;
        Loader loader = null;
        ClassLoader classLoader = null;
        ObjectInputStream ois = null;
        BufferedInputStream bis = null;
        Container container = manager.getContainer();
        String loadSql =
            "SELECT " + sessionIdCol + ", " + sessionDataCol + " FROM " +
            sessionTable + " WHERE " + sessionIdCol + " = ? AND " +
            sessionAppCol + " = ?";

        synchronized(this) {
            Connection _conn = getConnection();
            if(_conn == null) {
                return(null);
            }

            try {
                if(preparedLoadSql == null) {
                    preparedLoadSql = _conn.prepareStatement(loadSql);
                }

                preparedLoadSql.setString(1, id);
                preparedLoadSql.setString(2, getName());
                rst = preparedLoadSql.executeQuery();
                if (rst.next()) {
                    bis = new BufferedInputStream(rst.getBinaryStream(2));

                    if (container != null) {
                        loader = container.getLoader();
                    }
                    if (loader != null) {
                        classLoader = loader.getClassLoader();
                    }
                    if (classLoader != null) {
                        ois = new CustomObjectInputStream(bis,
                                                          classLoader);
                    } else {
                        ois = new ObjectInputStream(bis);
                    }

                    if (debug > 0) {
                        log(sm.getString(getStoreName()+".loading",
                                         id, sessionTable));
                    }

                    _session = (StandardSession) manager.createEmptySession();
                    _session.readObjectData(ois);
                    _session.setManager(manager);

                } else if (debug > 0) {
                    log(getStoreName()+": No persisted data object found");
                }
            } catch(SQLException e) {
                log(sm.getString(getStoreName()+".SQLException", e));
            } finally {
                try {
                    if(rst != null) {
                        rst.close();
                    }
                } catch(SQLException e) {
                    ;
                }
                if (ois != null) {
                    try {
                        ois.close();
                    } catch (IOException e) {
                        ;
                    }
                }
                release(_conn);
            }
        }

        return(_session);
    }

    /**
     * Remove the Session with the specified session identifier from
     * this Store, if present.  If no such Session is present, this method
     * takes no action.
     *
     * @param id Session identifier of the Session to be removed
     *
     * @exception IOException if an input/output error occurs
     */
    public void remove(String id) throws IOException {

        if (id == null) {
            return;
        }

        String removeSql =
            "DELETE FROM " + sessionTable + " WHERE " + sessionIdCol +
            " = ?  AND " + sessionAppCol + " = ?";

        synchronized(this) {
            Connection _conn = getConnection();

            if(_conn == null) {
                return;
            }

            try {
                if(preparedRemoveSql == null) {
                    preparedRemoveSql = _conn.prepareStatement(removeSql);
                }

                preparedRemoveSql.setString(1, id);
                preparedRemoveSql.setString(2, getName());
                preparedRemoveSql.execute();
            } catch(SQLException e) {
                log(sm.getString(getStoreName()+".SQLException", e));
            } finally {
                release(_conn);
            }
        }

        if (debug > 0) {
            log(sm.getString(getStoreName()+".removing", id, sessionTable));
        }
    }

    /**
     * Remove all of the Sessions in this Store.
     *
     * @exception IOException if an input/output error occurs
     */
    public void clear() throws IOException {
        String clearSql =
            "DELETE FROM " + sessionTable + " WHERE " + sessionAppCol + " = ?";

        synchronized(this) {
            Connection _conn = getConnection();
            if(_conn == null) {
                return;
            }

            try {
                if(preparedClearSql == null) {
                    preparedClearSql = _conn.prepareStatement(clearSql);
                }

                preparedClearSql.setString(1, getName());
                preparedClearSql.execute();
            } catch(SQLException e) {
                log(sm.getString(getStoreName()+".SQLException", e));
            } finally {
                release(_conn);
            }
        }
    }

    /**
     * Save a session to the Store.
     *
     * @param session the session to be stored
     * @exception IOException if an input/output error occurs
     */
    public void save(Session session) throws IOException {
        String saveSql =
            "INSERT INTO " + sessionTable + " (" + sessionIdCol + ", " +
            sessionAppCol + ", " +
            sessionDataCol + ", " +
            sessionValidCol + ", " +
            sessionMaxInactiveCol + ", " +
            sessionLastAccessedCol + ") VALUES (?, ?, ?, ?, ?, ?)";
        ObjectOutputStream oos = null;
        ByteArrayOutputStream bos = null;
        ByteArrayInputStream bis = null;
        InputStream in = null;

        synchronized(this) {
            Connection _conn = getConnection();
            if(_conn == null) {
                return;
            }

            // If sessions already exist in DB, remove and insert again.
            // TODO:
            // * Check if ID exists in database and if so use UPDATE.
            remove(session.getId());

            try {
                bos = new ByteArrayOutputStream();
                oos = new ObjectOutputStream(new BufferedOutputStream(bos));

                ((StandardSession)session).writeObjectData(oos);
                oos.close();

                byte[] obs = bos.toByteArray();
                int size = obs.length;
                bis = new ByteArrayInputStream(obs, 0, size);
                in = new BufferedInputStream(bis, size);

                if(preparedSaveSql == null) {
                    preparedSaveSql = _conn.prepareStatement(saveSql);
                }

                preparedSaveSql.setString(1, session.getId());
                preparedSaveSql.setString(2, getName());
                preparedSaveSql.setBinaryStream(3, in, size);
                preparedSaveSql.setString(4, session.isValid()?"1":"0");
                preparedSaveSql.setInt(5, session.getMaxInactiveInterval());
                preparedSaveSql.setLong(6, ((StandardSession)session).getLastUsedTime());
                preparedSaveSql.execute();
            } catch(SQLException e) {
                log(sm.getString(getStoreName()+".SQLException", e));
            } catch (IOException e) {
                ;
            } catch (ConcurrentModificationException e) {
                log(sm.getString(getStoreName()+".ConcurrentModificationException", e));
            } finally {
                if(bis != null) {
                    bis.close();
                }
                if(in != null) {
                    in.close();
                }

                release(_conn);
            }
        }

        if (debug > 0) {
            log(sm.getString(getStoreName()+".saving",
                             session.getId(), sessionTable));
        }
    }

    // --------------------------------------------------------- Protected Methods

    /**
     * Called by our background reaper thread to check if Sessions
     * saved in our store are subject of being expired. If so expire
     * the Session and remove it from the Store.
     *
     */
    protected void processExpires() {

        if(!started) {
            return;
        }

        String expiresSql =
            "SELECT " + sessionIdCol + " FROM " + sessionTable +
            " WHERE " + sessionAppCol + " = ? AND ? > (" +
            sessionLastAccessedCol + " + (" + sessionMaxInactiveCol +
            "*1000))";

        ResultSet rst = null;
        String keys[] = null;
        long timeNow = System.currentTimeMillis();

        synchronized(this) {
            Connection _conn = getConnection();

            if(_conn == null) {
                return;
            }

            try {
                if(preparedExpiresSql == null) {
                    preparedExpiresSql = _conn.prepareStatement(expiresSql);
                }

                preparedExpiresSql.setString(1, getName());
                preparedExpiresSql.setLong(2,timeNow);
                rst = preparedExpiresSql.executeQuery();
                ArrayList tmpkeys = new ArrayList();
                if (rst != null) {
                    while(rst.next()) {
                        tmpkeys.add(rst.getString(1));
                    }
                }
                keys = (String[]) tmpkeys.toArray(new String[tmpkeys.size()]);
            } catch(SQLException e) {
                log(sm.getString(getStoreName()+".SQLException", e));
                keys = new String[0];
            } finally {
                try {
                    if(rst != null) {
                        rst.close();
                    }
                } catch(SQLException e) {
                    ;
                }

                release(_conn);
            }
        }

        for (int i = 0; i < keys.length; i++) {
            try {
                StandardSession session = (StandardSession) load(keys[i]);
                if (session == null) {
                    continue;
                }
                if (!session.isValid()) {
                    continue;
                }
                int maxInactiveInterval = session.getMaxInactiveInterval();
                if (maxInactiveInterval < 0) {
                    continue;
                }
                int timeIdle = // Truncate, do not round up
                    (int) ((timeNow - ((StandardSession)session).getLastUsedTime()) / 1000L);
                if (timeIdle >= maxInactiveInterval) {
                    if ( ( (PersistentManagerBase) manager).isLoaded( keys[i] )) {
                        // recycle old backup session
                        session.recycle();
                    } else {
                        // expire swapped out session
                        session.expire();
                    }
                    remove(session.getId());
                }
            } catch (IOException e) {
                log (e.toString());
                e.printStackTrace();
            } catch (ClassNotFoundException e) {
                log (e.toString());
                e.printStackTrace();
            }
        }
    }

    /**
     * Check the connection associated with this store, if it's
     * <code>null</code> or closed try to reopen it.
     * Returns <code>null</code> if the connection could not be established.
     *
     * @return <code>Connection</code> if the connection suceeded
     */
    protected Connection getConnection(){
        try {
            if(conn == null || conn.isClosed()) {
                Class.forName(driverName);
                log(sm.getString(getStoreName()+".checkConnectionDBClosed"));
                conn = DriverManager.getConnection(connString);
                conn.setAutoCommit(true);

                if(conn == null || conn.isClosed()) {
                    log(sm.getString(getStoreName()+".checkConnectionDBReOpenFail"));
                }
            }
        } catch (SQLException ex){
            log(sm.getString(getStoreName()+".checkConnectionSQLException",
                             ex.toString()));
        } catch (ClassNotFoundException ex) {
            log(sm.getString(getStoreName()+".checkConnectionClassNotFoundException",
                             ex.toString()));
        }

        return conn;
    }

    /**
     * Release the connection, not needed here since the
     * connection is not associated with a connection pool.
     *
     * @param conn The connection to be released
     */
    protected void release(Connection conn) {
        ;
    }

    /**
     * Called once when this Store is first started.
     */
    public void start() throws LifecycleException {
        super.start();

        // Open connection to the database
        this.conn = getConnection();
    }

    /**
     * Gracefully terminate everything associated with our db.
     * Called once when this Store is stopping.
     *
     */
    public void stop() throws LifecycleException {
        super.stop();

        // Close and release everything associated with our db.
        if(conn != null) {
            try {
                conn.commit();
            } catch (SQLException e) {
                ;
            }

            if( preparedSizeSql != null ) {
                try {
                    preparedSizeSql.close();
                } catch (SQLException e) {
                    ;
                }
            }

            if( preparedKeysSql != null ) { 
                try {
                    preparedKeysSql.close();
                } catch (SQLException e) {
                    ;
                }
            }

            if( preparedSaveSql != null ) { 
                try {
                    preparedSaveSql.close();
                } catch (SQLException e) {
                    ;
                }
            }

            if( preparedClearSql != null ) { 
                try {
                    preparedClearSql.close();
                } catch (SQLException e) {
                    ;
                }
            }

            if( preparedRemoveSql != null ) { 
                try {
                    preparedRemoveSql.close();
                } catch (SQLException e) {
                    ;
                }
            }

            if( preparedLoadSql != null ) { 
                try {
                    preparedLoadSql.close();
                } catch (SQLException e) {
                    ;
                }
            }

            if( preparedExpiresSql != null ) {
                try {
                    preparedExpiresSql.close();
                } catch (SQLException e) {
                    ;
                }
            }

            try {
                conn.close();
            } catch (SQLException e) {
                ;
            }

            this.preparedSizeSql = null;
            this.preparedKeysSql = null;
            this.preparedSaveSql = null;
            this.preparedClearSql = null;
            this.preparedRemoveSql = null;
            this.preparedLoadSql = null;
            this.preparedExpiresSql = null;
            this.conn = null;
        }
    }
}
