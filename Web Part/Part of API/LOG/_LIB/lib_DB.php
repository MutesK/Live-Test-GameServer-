<?php

 include "_Config_DB.php";
 include "lib_Log.php";

    function DBConnect()
    {
        global $DB_IP;
        global $DB_ID;
        global $DB_PASS;
        global $DB_NAME;

        global $g_DB;
        global $g_AccountNo;
    
        if(!isset($g_AccountNo))
            $g_AccountNo = -1;

        printf("IP : %s, ID : %s, PASS : %s, Name : %s \n", $DB_IP, $DB_ID, $DB_PASS, $DB_NAME);

        $g_DB = mysqli_connect($DB_IP, $DB_ID, $DB_PASS, $DB_NAME);
 
        if(!$g_DB)
        {
            printf("DB Error : %s\n",  mysqli_connect_error());
            LOG_System($g_AccountNo, "MySQL Connect", "{$_SERVER['PHP_SELF']} -> lib_DB.php => ERROR# mysqli_connect() :". mysqli_connect_error(), 3);
            QuitError("Server Error[DBC]");
            exit;
        }

        // 언어셋 맞춤
        mysqli_set_charset($g_DB, "utf8");
    }

    function DBDisconnect()
    {
        global $g_DB;

         if(isset($g_DB))
            mysqli_close($g_DB);
    }

    function DB_ExecQuery($Query)
    {
        global $g_DB;
        global $g_AccountNo;

        if(!isset($g_AccountNo))
            $g_AccountNo = -1;

        $Result = mysqli_query($g_DB, $Query);

        if(!$Result)
        {
            LOG_System($g_AccountNo, "DB ExecQuery", "{$_SERVER['PHP_SELF']} -> lib_DB.php => DB Query Error : ". mysqli_error($g_DB), 3);
            QuitError("Server Error[DBQ]");
        }

        return $Result;
    }

    // 트랜젝션 쿼리
    function DB_TransectionQuery($qryArr)
    {
        global $g_DB;
        global $g_AccountNo;

        if(!isset($g_AccountNo))
            $g_AccountNo = -1;

        $insert_idArr = array();

        mysqli_begin_transaction($g_DB);

        foreach($qryArr as $value)
        {
            if(is_string($value))
            {
               // LOG_System($g_AccountNo, "{$_SERVER['PHP_SELF']} > DB_TransectionQuery()", "QUERY:". $value);

                if(!mysqli_query($g_DB, $value))
                {
                    LOG_System($g_AccountNo, "{$_SERVER['PHP_SELF']} > DB_TransectionQuery()", "QUERY Error : ".$value ." /". mysqli_error($g_DB));
                    DB_TransectionFail();
                    exit;
                }

                array_push($insert_idArr, mysqli_insert_id($g_DB));
            }
            else
            {
                LOG_System($g_AccountNo, "{$_SERVER['PHP_SELF']} > DB_TransectionQuery()", "QUERY Error : ".$value ." /". mysqli_error($g_DB));
                echo "DB Query Error :". mysqli_error($g_DB);
                
                DB_TransectionFail();
                exit;
            }
        }

        mysqli_commit($g_DB);
        return $insert_idArr;
    }

    function DB_TransectionFail()
    {
        global $g_DB;
        mysqli_rollback($g_DB);
    }
?>