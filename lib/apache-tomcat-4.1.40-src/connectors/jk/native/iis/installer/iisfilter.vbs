'
' Licensed to the Apache Software Foundation (ASF) under one or more
' contributor license agreements.  See the NOTICE file distributed with
' this work for additional information regarding copyright ownership.
' The ASF licenses this file to You under the Apache License, Version 2.0
' (the "License"); you may not use this file except in compliance with
' the License.  You may obtain a copy of the License at
'
'     http://www.apache.org/licenses/LICENSE-2.0
'
' Unless required by applicable law or agreed to in writing, software
' distributed under the License is distributed on an "AS IS" BASIS,
' WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
' See the License for the specific language governing permissions and
' limitations under the License.
'

' =========================================================================
' Description: Install script for Tomcat ISAPI redirector                              
' Author:      Mladen Turk <mturk@apache.org>                           
' Version:     $Revision: 572120 $                                           
' =========================================================================

'
' Get a handle to the filters for the server - we process all errors
'
On Error Resume Next

filterName = "jakarta"
filterLib = "bin\isapi_redirect.dll"
  
Function IISInstallFilter(filterDir, filterObject)

    Dim filters
    Set filters = GetObject(filterObject)
    If err Then err.clear
    info "Got Filters " + filters.FilterLoadOrder
    
    '
    ' Create the filter - if it fails then delete it and try again
    '
    name = filterName
    info "Creating Filter  - " + filterName
    Dim filter
    Set filter = filters.Create( "IISFilter", filterName )
    If err Then
    	err.clear
    	info "Filter exists - deleting"
    	filters.delete "IISFilter", filterName
    	If err Then
    	    info "Error Deleting Filter"
    	    IISInstallFilter = 0
    	    Exit Function
    	End If
    	Set filter = filters.Create( "IISFilter", filterName )
    	If Err Then
    	    info "Error Creating Filter"
    	    IISInstallFilter = 0
    	    Exit Function
    	End If
    End If
    
    '
    ' Set the filter info and save it
    '
    filter.FilterPath = filterDir + filterLib
'    filter.FilterEnabled = true
    filter.FilterDescription = "Jakarta Isapi Redirector"
    filter.NotifyOrderHigh = true
    filter.SetInfo
    info "Created Filter " + filterDir + filterLib
    
    '
    ' Set the load order - only if it's not in the list already
    '
    On Error goto 0
    loadOrders = filters.FilterLoadOrder
    list = Split( loadOrders, "," )
    found = false
    For each item in list
    	If Trim( item ) = filterName Then found = true
    Next
    
    If found = false Then 
    	info "Filter is not in load order - adding now."
    	If Len(loadOrders) <> 0  Then loadOrders = loadOrders + ","
    	filters.FilterLoadOrder = loadOrders + filterName
    	filters.SetInfo
    	info "Added Filter " + filterName 
    Else
    	info "Filter already exists in load order - no update required."
    End If
    IISInstallFilter = 1
End Function

' 
' Helper function for snafus
'
Function fail(message)
'	MsgBox " " + message
	WScript.Quit(1)
End function

'
' Helper function for info
'
Function info(message)
'	MsgBox " " + message
End Function 

info "Installing IIS Filter " + Session.Property("INSTALLDIR")
Dim rv
rv = 0
rv = IISInstallFilter(Session.Property("INSTALLDIR"), "IIS://LocalHost/W3SVC/1/Filters")
If rv = 0 Then    
    rv = IISInstallFilter(Session.Property("INSTALLDIR"), "/LM/W3SVC/Filters")
End If
If rv = 0 Then
    rv = IISInstallFilter(Session.Property("INSTALLDIR"), "/LM/W3SVC/1/Filters")
End If
