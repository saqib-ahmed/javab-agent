mode(0)
clear
 
//Data file location
filedir = "/home/saqib/dev_tools/Documentation/par_codes/";
filename = "multi_dim.csv";
 
//Define colors and descriptions for 3 columns of data
descriptions = {"Sequential","Fork Join","JAVAB", "Executor Service"};
 dim = 200:20:1200;
//Get data from csv-file with European formating (; separator)
data_values = csvRead(filedir + filename,",");
 
//Create figure and loop through all columns and plot all data agains first column.
f = scf();
 //Plot columns, with different colors
plot(dim,data_values(1,:),'g-o');
plot(dim,data_values(2,:),'r-o');
plot(dim,data_values(3,:),'b-o');
plot(dim,data_values(4,:),'k-x');
plot(dim,data_values(5,:),'m-x');
//Set other figure graphics
title("Java Multithreading Topologies (Server- 48 cores 64GB RAM 48 Threads)")
xlabel("Dimensions of Matrix");
ylabel("Time [s]");
hl=legend(["Sequential (tiered)";"Fork Join";"JAVAB";"Sequential (c1 only)"; "Fork/Join with dynamic linking"], 2);
