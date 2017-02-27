mode(0)
clear
 
//Data file location
filedir = "/home/saqib/dev_tools/Documentation/par_codes/";
filename = "pc_dev_jvm_3.csv";
 
//Define colors and descriptions for 3 columns of data
descriptions = {"Sequential","Fork Join","JAVAB", "Executor Service"};
 
//Get data from csv-file with European formating (; separator)
data_values = csvRead(filedir + filename,",");
 
//Create figure and loop through all columns and plot all data agains first column.
f = scf();
 //Plot columns, with different colors
 bar(1,mean(data_values(:,2)),'g');
 bar(4,mean(data_values(:,3)),'r');
 bar(6,mean(data_values(:,4)),'b');
 bar(3,mean(data_values(:,5)),'c');
 bar(2,mean(data_values(:,6)),'k');
 bar(5,mean(data_values(:,7)),'m');

filename = "laptop_dev_jvm_2.csv";
 
//Define colors and descriptions for 3 columns of data
descriptions = {"Sequential","Fork Join","JAVAB", "Executor Service"};
 
//Get data from csv-file with European formating (; separator)
data_values = csvRead(filedir + filename,",");
 
 
 //Plot columns, with different colors
 bar(10,mean(data_values(:,2)),'g');
 bar(13,mean(data_values(:,3)),'r');
 bar(15,mean(data_values(:,4)),'b');
 bar(12,mean(data_values(:,5)),'c');
 bar(11,mean(data_values(:,6)),'k');
 bar(14,mean(data_values(:,7)),'m');
 
 filename = "server_dev_jvm_2.csv";
 
//Define colors and descriptions for 3 columns of data
descriptions = {"Sequential","Fork Join","JAVAB", "Executor Service"};
 
//Get data from csv-file with European formating (; separator)
data_values = csvRead(filedir + filename,",");
 
 //Plot columns, with different colors
 bar(20,mean(data_values(:,2)),'g');
 bar(21,mean(data_values(:,3)),'r');
 bar(23,mean(data_values(:,4)),'b');
 bar(22,mean(data_values(:,5)),'c');
 bar(19,mean(data_values(:,6)),'k');
 bar(24,mean(data_values(:,7)),'m');
 xgrid();
//Set other figure graphics
title("Java Multithreading Topologies (summary)")
xlabel("PC                                                       Laptop                                                            Server");
ylabel("Time [s]");
hl=legend(["Sequential (Tiered Compiled)";"Fork Join";"JAVAB";"Executor Service";"Sequential (c1 only)"; "Fork/Join with dynamic linking"]);
