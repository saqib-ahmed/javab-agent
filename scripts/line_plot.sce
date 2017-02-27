mode(0)
clear
 
//Data file location
filedir = "/home/saqib/dev_tools/Documentation/par_codes/";
filename = "laptop_dev_jvm.csv";
 
//Define colors and descriptions for 3 columns of data
descriptions = {"Sequential","Fork Join","JAVAB", "Executor Service"};
 
//Get data from csv-file with European formating (; separator)
data_values = csvRead(filedir + filename,",");
 
//Create figure and loop through all columns and plot all data agains first column.
f = scf();
 //Plot columns, with different colors
 plot(data_values(:,1),data_values(:,2),'g-o');
plot(data_values(:,1),data_values(:,3),'r-o');
 plot(data_values(:,1),data_values(:,4),'b-o');
 plot(data_values(:,1),data_values(:,5),'c-o');
plot(data_values(:,1),data_values(:,6),'k-x');
plot(data_values(:,1),data_values(:,7),'m-x');
//Set other figure graphics
title("Java Multithreading Topologies (Laptop- 8 cores 8GB RAM)")
xlabel("No. of Threads");
ylabel("Time [s]");
hl=legend(["Sequential (only)";"Fork Join";"JAVAB";"Executor Service";"Sequential (with dynamic FJ)"; "Fork/Join with dynamic linking"]);
