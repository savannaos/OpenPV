
object_ids = [26:50];
object_name = cell(length(object_ids),1);
for i_object = 1 : length(object_name)
		 object_name{i_object} = num2str(object_ids(i_object), "%3.3i");		 
endfor

chip_path = "/mnt/data1/repo/neovision-data-challenge-heli/"
petavision_path = "/mnt/data1/repo/neovision-programs-petavision/Heli/Challenge/";
DoG_flag = 1;
DoG_struct = struct;  %% 
DoG_struct.amp_center_DoG = 1;
DoG_struct.sigma_center_DoG = 1;
DoG_struct.amp_surround_DoG = 1;
DoG_struct.sigma_surround_DoG = 2 * DoG_struct.sigma_center_DoG;
canny_flag = 1;
canny_struct = struct;  %% 
canny_struct.sigma_canny = 1;
pad_size = [1080 1920];
num_procs = 24;
for i_object = 1 : length(object_name)
  padChips(chip_path, ...
	   object_name{i_object}, ...
	   petavision_path, ...
	   DoG_flag, ...
	   DoG_struct, ...
	   canny_flag, ...
	   canny_struct, ...
	   pad_size, ...
	   num_procs)
endfor

num_train = -1;
skip_train_images = 1;
begin_train_images = 1;
train_dir = "canny";
list_dir = "list_canny2";
shuffle_flag = 0;

for i_object = 1 : length(object_name)
  chipFileOfFilenames(petavision_path, ...
		      object_name{i_object}, ...
		      num_train, ...
		      skip_train_images, ...
		      begin_train_images, ...
		      train_dir, ...
		      list_dir, ...
		      shuffle_flag, ...
		      []);
endfor

%% make activity dirs
for i_object = 1 : length(object_name)
  mkdir(["/mnt/data1/repo/neovision-programs-petavision/Heli/Challenge/activity/", object_name{i_object}, filesep])
  mkdir(["/mnt/data1/repo/neovision-programs-petavision/Heli/Challenge/activity/", object_name{i_object}, filesep, "Car3", filesep])
  mkdir(["/mnt/data1/repo/neovision-programs-petavision/Heli/Challenge/activity/", object_name{i_object}, filesep, "Car3", filesep, "canny2", filesep])
  
endfor

%% make params dirs
for i_object = 1 : 0%% length(object_name)
  %%mkdir(["~/workspace-indigo/Clique2/input/Heli/Challenge/", object_name{i_object}, filesep])
  %%mkdir(["~/workspace-indigo/Clique2/input/Heli/Challenge/", object_name{i_object}, filesep, "Car3", filesep])
  mkdir(["~/workspace-indigo/Clique2/input/Heli/Challenge/", object_name{i_object}, filesep, "Car3", filesep, "canny2", filesep])
endfor


%% copy params
base_dir = ["~/workspace-indigo/Clique2/input/Heli/Challenge/","007", filesep, "Car3", filesep];
base_name = ["Heli_", "007", "_Car3_"]; 
for i_object = 1 : 0 %% length(object_name)
  derived_dir = ["~/workspace-indigo/Clique2/input/Heli/Challenge/",object_name{i_object}, filesep, "Car3", filesep];
  derived_name = ["Heli_", object_name{i_object}, "_Car3_"]; 
  copyfile([base_dir, "canny", filesep, base_name, "canny", ".params"], [derived_dir, "canny", filesep, derived_name, "canny", ".params"])
endfor
