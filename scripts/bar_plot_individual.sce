mode(0)
clear
 
//Data file location
filedir = "/home/saqib/dev_tools/Documentation/par_codes/";
filename = "pc_dev_jvm_2.csv";
 
//Define colors and descriptions for 3 columns of data
descriptions = {"Sequential","Fork Join","JAVAB", "Executor Service"};
 
//Get data from csv-file with European formating (; separator)
data_values = csvRead(filedir + filename,",");
 
seq1(1) = mean(data_values(:,2));
FJ(1) = mean(data_values(:,3));
JAVAB(1) = mean(data_values(:,4));
ES(1) = mean(data_values(:,5));
seq2(1) = mean(data_values(:,6));
FJ_DL(1) = mean(data_values(:,7));
x_axis = [1:3;4:6;7:9;10:12;13:15;16:18];
filename = "laptop_dev_jvm.csv";
 
//Get data from csv-file with European formating (; separator)
data_values = csvRead(filedir + filename,",");

seq1(2) = mean(data_values(:,2));
FJ(2) = mean(data_values(:,3));
JAVAB(2) = mean(data_values(:,4));
ES(2) = mean(data_values(:,5));
seq2(2) = mean(data_values(:,6));
FJ_DL(2) = mean(data_values(:,7));
 
 
filename = "server_dev_jvm.csv";
 
//Get data from csv-file with European formating (; separator)
data_values = csvRead(filedir + filename,",");
 
seq1(3) = mean(data_values(:,2));
FJ(3) = mean(data_values(:,3));
JAVAB(3) = mean(data_values(:,4));
ES(3) = mean(data_values(:,5));
seq2(3) = mean(data_values(:,6));
FJ_DL(3) = mean(data_values(:,7));

//Create figure and loop through all columns and plot all data agains first column.
f = scf();

 //Plot columns, with different colors
 bar(x_axis(1,:),seq1,'g');
 bar(x_axis(2,:),FJ,'r');
 bar(x_axis(3,:),JAVAB,'b');
 bar(x_axis(4,:),ES,'c');
 bar(x_axis(5,:),seq2,'k');
 bar(x_axis(6,:),FJ_DL,'m');
  xgrid();
//Set other figure graphics
title("Java Multithreading Topologies (summary)")
xlabel("PC----Laptop----Server");
ylabel("Time [s]");
hl=legend(["Sequential (Tiered)";"Fork Join";"JAVAB";"Executor Service";"Sequential (c1 only)"; "Fork/Join with dynamic linking"]);
