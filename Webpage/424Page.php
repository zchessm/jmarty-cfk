<?php


/*if(!isset ($_SESSION['username'])) {
		echo "<meta http-equiv=\"refresh\" content=\"0;url=login.php\">";
}*/


$query = "SELECT * FROM Iterations";
$result = mysqli_query($connection, $query);


// Printing results in HTML
echo "<h1>Database Contents</h1>";
echo "<table>\n";
echo "\t<tr>\n \t\t<td>IP</td>\t\t<td>Time</td>\t\t<td>RTTMin</td>\t\t<td>RTTMax</td>\t\t<td>RTTAverage</td>\t\t<td>ID</td>\n";
while ($line = mysqli_fetch_array($result, MYSQLI_ASSOC)) {
   echo "\t<tr>\n";
   foreach ($line as $col_value) {
      echo "\t\t<td>$col_value</td>\n";
   }
   echo "\t</tr>\n";
}

echo "</table>\n";

?>
