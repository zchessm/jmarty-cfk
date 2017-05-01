<?php
include "mysqlClass.inc.php";


function user_pass_check($username, $password)
{
	
	$query = "select * from account where username='$username'";
	$result = mysql_query( $query );
		
	if (!$result)
	{
	   die ("user_pass_check() failed. Could not query the database: <br />". mysql_error());
	}
	else{
		$row = mysql_fetch_row($result);
		if(strcmp($row[1],$password))
			return 2; //wrong username
		else 
			return 0; //Checked.
	}	
}

?>
