dnl
dnl Licensed to the Apache Software Foundation (ASF) under one or more
dnl contributor license agreements.  See the NOTICE file distributed with
dnl this work for additional information regarding copyright ownership.
dnl The ASF licenses this file to You under the Apache License, Version 2.0
dnl (the "License"); you may not use this file except in compliance with
dnl the License.  You may obtain a copy of the License at
dnl
dnl     http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl Unless required by applicable law or agreed to in writing, software
dnl distributed under the License is distributed on an "AS IS" BASIS,
dnl WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl See the License for the specific language governing permissions and
dnl limitations under the License.
dnl

dnl --------------------------------------------------------------------------
dnl Author Henri Gomez <hgomez@apache.org>
dnl
dnl Inspired by Pier works on webapp m4 macros :)
dnl 
dnl Version $Id: jk_tchome.m4 466585 2006-10-21 22:16:34Z markt $
dnl --------------------------------------------------------------------------

dnl --------------------------------------------------------------------------
dnl JK_TCHOME
dnl   Set the Tomcat Home directory.
dnl   $1 => Tomcat Name
dnl   $2 => Tomcat VarName
dnl   $3 => File which should be present
dnl --------------------------------------------------------------------------
AC_DEFUN(
  [JK_TCHOME],
  [
    tempval=""

    AC_MSG_CHECKING([for $1 location])
    AC_ARG_WITH(
      [$1],
      [  --with-$1=DIR      Location of $1 ],
      [ 
        case "${withval}" in
        ""|"yes"|"YES"|"true"|"TRUE")
          ;;
        "no"|"NO"|"false"|"FALSE")
          AC_MSG_ERROR(valid $1 location required)
          ;;
        *)
          tempval="${withval}"

          if ${TEST} ! -d ${tempval} ; then
            AC_MSG_ERROR(Not a directory: ${tempval})
          fi

          if ${TEST} ! -f ${tempval}/$3; then
            AC_MSG_ERROR(can't locate ${tempval}/$3)
          fi
          ;;
        esac
      ])  

      if ${TEST} -z "$tempval" ; then
        AC_MSG_RESULT(not provided)
      else
        [$2]=${tempval}
        AC_MSG_RESULT(${[$2]})
      fi

      unset tempval
  ])

dnl vi:set sts=2 sw=2 autoindent:
