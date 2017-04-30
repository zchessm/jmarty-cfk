
<?php
//DB Connect
$connection = mysqli_connect('mysql1.cs.clemson.edu','khattle','420noscopeblazeit');
mysqli_select_db($connection, 'ssadatabase');
$salt = 'b0t@t0';

$query = "SELECT *, 
AES_DECRYPT(IP, '.$salt.') AS IP_clear,
AES_DECRYPT(Time, '.$salt.') AS Time_clear,
AES_DECRYPT(RTTMin, '.$salt.') AS RTTMin_clear,
AES_DECRYPT(RTTMax, '.$salt.') AS RTTMax_clear,
AES_DECRYPT(RTTAverage, '.$salt.') AS RTTAvg_clear,
AES_DECRYPT(ID, '.$salt.') AS ID_clear
FROM Iterations";
$result = mysqli_query($connection, $query);


// Printing results in HTML
echo "<h1>Database Contents</h1>";
echo "<table>\n";
echo "\t<tr>\n \t\t<td>IP</td>\t\t<td>Time</td>\t\t<td>RTTMin</td>\t\t<td>RTTMax</td>\t\t<td>RTTAverage</td>\t\t<td>ID</td>\n";

while ($line = mysqli_fetch_array($result, MYSQLI_ASSOC)) {
   echo "\t<tr>\n";
	echo "\t\t<td>" . $line['IP_clear'] . "</td>\n";
   echo "\t\t<td>" . $line['Time_clear'] . "</td>\n";
	echo "\t\t<td>" . $line['RTTMin_clear'] . "</td>\n";
	echo "\t\t<td>" . $line['RTTMax_clear'] . "</td>\n";
	echo "\t\t<td>" . $line['RTTAvg_clear'] . "</td>\n";
	echo "\t\t<td>" . $line['ID_clear'] . "</td>\n";
   echo "\t</tr>\n";
}

echo "</table>\n";

mysqli_free_result($result);

mysqli_close($connection);
?>
