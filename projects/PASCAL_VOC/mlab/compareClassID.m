%% Quick and dirty visualization harness for ground truth pvp files
%% each classID is assigned a different color
%% where bounding boxes overlapp, the color is a mixture
%% use this script to visualize ground truth sparse pvp files and for comparison with 
%% original images to verify that the bounding box annotations are reasonable
%% hit any key to advance to the next image
%% now augmented by a confusion matrix

%%HELLO

close all
more off
pkg load all
setenv("GNUTERM","X11")
%%addpath("~/workspace/PetaVision/mlab/imgProc");
%%addpath("~/workspace/PetaVision/mlab/util");
%%addpath("~/workspace/PetaVision/mlab/HyPerLCA");

plot_flag = true;
%%output_dir = "VOC2007_landscape4bAWS";
output_dir = "VOC2007_portrait4bAWS";
%%output_dir = "/nh/compneuro/Data/PASCAL_VOC/PASCAL_S1_96_S2_1536_SumMaxPooled_16X12_4X3_SLP/VOC2007_landscape2";

%%draw reconstructed image
DoG_weights = [];
Recon_list = {["a0_"],  ["Image"]};
%% list of layers to unwhiten
num_Recon_list = size(Recon_list,1);
Recon_unwhiten_list = zeros(num_Recon_list,1);
%% list of layers to use as a normalization reference for unwhitening
Recon_normalize_list = 1:num_Recon_list;
%% list of (previous) layers to sum with current layer
Recon_sum_list = cell(num_Recon_list,1);
num_Recon_frames_per_layer = 1;
Recon_LIFO_flag = true;
[Recon_hdr, Recon_fig,  Recon_fig_name, Recon_vals,  Recon_time, Recon_mean,  Recon_std] = analyzeUnwhitenedReconPVP(Recon_list, num_Recon_frames_per_layer, output_dir, plot_flag, Recon_sum_list, Recon_LIFO_flag);
drawnow;

%% sparse activity
Sparse_list ={["a4_"], ["GroundTruth_16X12"]; ["a15_"], ["GroundTruth_4X3"]}; 
%%Sparse_list ={["a10_"], ["GroundTruth_16X12"]; ["a16_"], ["GroundTruth_4X3"]}; 
fraction_Sparse_frames_read = 1;
min_Sparse_skip = 1;
fraction_Sparse_progress = 10;
num_epochs = 1;
num_procs = 1;
Sparse_frames_list = [];
load_Sparse_flag = false;
[Sparse_hdr, Sparse_hist_rank_array, Sparse_times_array, Sparse_percent_active_array, Sparse_percent_change_array, Sparse_std_array, Sparse_struct_array] = analyzeSparseEpochsPVP2(Sparse_list, output_dir, load_Sparse_flag, plot_flag, fraction_Sparse_frames_read, min_Sparse_skip, fraction_Sparse_progress, Sparse_frames_list, num_procs, num_epochs);
drawnow;


				%pause;

%% Error vs time
nonSparse_list = {["a6_"], ["GroundTruthReconS1Error_16X12"]; ["a11_"], ["GroundTruthReconS2Error_16X12"]; ["a17_"], ["GroundTruthReconS1Error_4X3"]; ["a22_"], ["GroundTruthReconS2Error_4X3"]}; 
%%nonSparse_list = {["a12_"], ["GroundTruthError_16X12"]; ["a18_"], ["GroundTruthError_4X3"]}; 
num_nonSparse_list = size(nonSparse_list,1);
nonSparse_skip = repmat(1, num_nonSparse_list, 1);
nonSparse_norm_list = {["a3_"], ["GroundTruth_16X12"]; ["a3_"], ["GroundTruth_16X12"]; ["a15_"], ["GroundTruth_4X3"]; ["a15_"], ["GroundTruth_4X3"]}; 
%%nonSparse_norm_list = {["a10_"], ["GroundTruth_16X12"]; ["a16_"], ["GroundTruth_4X3"]}; 
nonSparse_norm_strength = ones(num_nonSparse_list,1);
Sparse_std_ndx = [1 2 1 2]; %% 
fraction_nonSparse_frames_read = 1;
min_nonSparse_skip = 1;
fraction_nonSparse_progress = 10;
[nonSparse_times_array, nonSparse_RMS_array, nonSparse_norm_RMS_array, nonSparse_RMS_fig] = analyzeNonSparsePVP(nonSparse_list, nonSparse_skip, nonSparse_norm_list, nonSparse_norm_strength, Sparse_times_array, Sparse_std_array, Sparse_std_ndx, output_dir, plot_flag, fraction_nonSparse_frames_read, min_nonSparse_skip, fraction_nonSparse_progress);
for i_nonSparse = 1 : num_nonSparse_list
  figure(nonSparse_RMS_fig(i_nonSparse));
  grid on
  set(gca, 'linewidth', 2.0, 'color', [1 1 1]);
endfor
drawnow;
				%pause;


classes={...
         'aeroplane'
         'bicycle'
         'bird'
         'boat'
         'bottle'
         'bus'
         'car'
         'cat'
         'chair'
         'cow'
         'diningtable'
         'dog'
         'horse'
         'motorbike'
         'person'
         'pottedplant'
         'sheep'
         'sofa'
         'train'
         'tvmonitor'};

JIEDDO_class_ndx = [2 6 7 14 15 19];
JIEDDO_class_ndx_Shift = JIEDDO_class_ndx + 1;
JIEDDO_classes = classes(JIEDDO_class_ndx)


for i_scale = 4 : 4  %% gjkscale
  if i_scale == 1
    pred_classID_file = fullfile("VOC2007_portrait4bAWS/a5_GroundTruthReconS1_16X12.pvp")
    gt_classID_file = fullfile("VOC2007_portrait4bAWS/a4_GroundTruth_16X12.pvp")
  elseif i_scale == 2
    pred_classID_file = fullfile("VOC2007_portrait4bAWS/a10_GroundTruthReconS2_16X12.pvp")
    gt_classID_file = fullfile("VOC2007_portrait4bAWS/a4_GroundTruth_16X12.pvp")
  elseif i_scale == 3
    pred_classID_file = fullfile("VOC2007_portrait4bAWS/a16_GroundTruthReconS1_4X3.pvp")
    gt_classID_file = fullfile("VOC2007_portrait4bAWS/a15_GroundTruth_4X3.pvp")
  elseif i_scale == 4
    pred_classID_file = fullfile("VOC2007_portrait4bAWS/a21_GroundTruthReconS2_4X3.pvp")
    gt_classID_file = fullfile("VOC2007_portrait4bAWS/a15_GroundTruth_4X3.pvp")
  endif
  pred_classID_fid = fopen(pred_classID_file);
  pred_classID_hdr = readpvpheader(pred_classID_fid);
  fclose(pred_classID_fid);
  tot_pred_classID_frames = pred_classID_hdr.nbands;
  [pred_data,pred_hdr] = readpvpfile(pred_classID_file, ceil(tot_pred_classID_frames/1), tot_pred_classID_frames, 1, 1); 
  gt_classID_fid = fopen(gt_classID_file);
  gt_classID_hdr = readpvpheader(gt_classID_fid);
  fclose(gt_classID_fid);
  tot_gt_classID_frames = gt_classID_hdr.nbands;
  [gt_data,gt_hdr] = readpvpfile(gt_classID_file, ceil(tot_gt_classID_frames/1), tot_gt_classID_frames, 1, 1); 
  %%[imageRecon_data,imageRecon_hdr] = readpvpfile(imageRecon_file); 
				%true_num_neurons = true_hdr.nf * true_hdr.nx * true_hdr.ny;
				%true_num_frames = length(true_data);
  pred_num_neurons = pred_hdr.nf * pred_hdr.nx * pred_hdr.ny;
  pred_num_frames = length(pred_data);
  gt_num_neurons = gt_hdr.nf * gt_hdr.nx * gt_hdr.ny;
  gt_num_frames = length(gt_data);
  %%imageRecon_num_neurons = imageRecon_hdr.nf * imageRecon_hdr.nx * imageRecon_hdr.ny;
  %%imageRecon_num_frames = length(imageRecon_data);
  classID_hist_bins = -0.25:0.01:2.0;
  num_classID_bins = length(classID_hist_bins);
  pred_classID_hist = zeros(num_classID_bins, length(JIEDDO_class_ndx),2);
  %%pred_classID_sum = zeros(length(JIEDDO_class_ndx), 1);
  %%pred_classID_sum2 = zeros(length(JIEDDO_class_ndx), 1);
  classID_colormap = prism(length(JIEDDO_class_ndx)+0); %%hot(gt_hdr.nf+1); %%rainbow(length(JIEDDO_class_ndx)); %%prism(length(JIEDDO_class_ndx));
  use_false_positive_thresh = false; %%true; %%
  false_positive_thresh = .99;
  for i_frame = 100 : min(pred_num_frames, gt_num_frames) 
    
    %% ground truth layer is sparse
    if mod(i_frame, ceil(gt_num_frames/10)) == 0
      display(["i_frame = ", num2str(i_frame)])
    endif
    gt_time = gt_data{i_frame}.time;
    gt_num_active = length(gt_data{i_frame}.values);
    gt_active_ndx = gt_data{i_frame}.values+1;
    gt_active_sparse = sparse(gt_active_ndx,1,1,gt_num_neurons,1,gt_num_active);
    gt_classID_cube = full(gt_active_sparse);
    gt_classID_cube = reshape(gt_classID_cube, [gt_hdr.nf, gt_hdr.nx, gt_hdr.ny]);
    gt_classID_cube = permute(gt_classID_cube, [3,2,1]);
    
    %% only display predictions for these frames
    if any(gt_time == Recon_time{1})
      display(["i_frame = ", num2str(i_frame)]);
      
      [gt_classID_val, gt_classID_ndx] = max(gt_classID_cube, [], 3);
      min_gt_classID = min(gt_classID_val(:))
      %%gt_classID_cube = gt_classID_cube .* (gt_classID_cube >= min_gt_classID);
      gt_classID_heatmap = zeros(gt_hdr.ny, gt_hdr.nx, 3);
      i_JIEDDO_classID = 0;
      for i_classID = JIEDDO_class_ndx %%1 : gt_hdr.nf
	i_JIEDDO_classID = i_JIEDDO_classID + 1;
	if ~any(any(gt_classID_cube(:,:,i_classID)))
	  continue;
	endif
	gt_class_color = classID_colormap(i_JIEDDO_classID, :); %%getClassColor(gt_class_color_code);
	gt_classID_band = repmat(gt_classID_cube(:,:,i_classID), [1,1,3]);
	gt_classID_band(:,:,1) = gt_classID_band(:,:,1) * gt_class_color(1)*255;
	gt_classID_band(:,:,2) = gt_classID_band(:,:,2) * gt_class_color(2)*255;
	gt_classID_band(:,:,3) = gt_classID_band(:,:,3) * gt_class_color(3)*255;
	gt_classID_heatmap = gt_classID_heatmap + gt_classID_band .* repmat(squeeze(sum(gt_classID_heatmap,3)==0),[1,1,3]);
      endfor
      gt_classID_heatmap = mod(gt_classID_heatmap, 256);
      if plot_flag
	gt_fig = figure("name", ["Ground Truth: ", num2str(gt_time, "%i")]);
	image(uint8(gt_classID_heatmap)); axis off; axis image, box off;
	drawnow
      endif
      imwrite(uint8(gt_classID_heatmap), [output_dir, filesep, 'Recon', filesep, "gt_", num2str(gt_time, "%i"), "_", num2str(i_scale), '.png'], 'png');
    endif
    
    %% recon layer is not sparse
    pred_time = pred_data{i_frame}.time;
    pred_classID_cube = pred_data{i_frame}.values;
    pred_classID_cube = permute(pred_classID_cube, [2,1,3]);
    JIEDDO_classID_cube = pred_classID_cube(:,:,JIEDDO_class_ndx);
    i_JIEDDO_classID = 0;
    for i_classID = JIEDDO_class_ndx %%1 : pred_hdr.nf
      i_JIEDDO_classID = i_JIEDDO_classID + 1;
      pred_classID_tmp = squeeze(pred_classID_cube(:,:,i_classID)); % gjk
      gt_classID_tmp = squeeze(gt_classID_cube(:,:,i_classID)); %% gjk
      pos_pred_tmp = pred_classID_tmp(gt_classID_tmp(:)~=0);
      neg_pred_tmp = pred_classID_tmp(gt_classID_tmp(:)==0);
      if any(pos_pred_tmp)
	pred_classID_hist(:,i_JIEDDO_classID,1) = squeeze(pred_classID_hist(:,i_JIEDDO_classID,1)) + hist(pos_pred_tmp(:), classID_hist_bins)';
      endif
      if any(neg_pred_tmp)
	pred_classID_hist(:,i_JIEDDO_classID,2) = squeeze(pred_classID_hist(:,i_JIEDDO_classID,2)) + hist(neg_pred_tmp(:), classID_hist_bins)';
      endif  
    endfor
    if any(pred_time == Recon_time{1}) 
      pred_time
      Recon_time
      pred_classID_cumsum = squeeze(cumsum(pred_classID_hist, 1));
      pred_classID_sum = squeeze(sum(pred_classID_hist, 1));
      pred_classID_norm = repmat(reshape(pred_classID_sum, [1,length(JIEDDO_class_ndx),2]), [num_classID_bins,1,1]);
      pred_classID_cumprob = pred_classID_cumsum ./ (pred_classID_norm + (pred_classID_norm==0));
      pred_classID_thresh_bin = zeros(length(JIEDDO_class_ndx),1);
      pred_classID_thresh = zeros(length(JIEDDO_class_ndx),1);
      pred_classID_true_pos = zeros(length(JIEDDO_class_ndx),1);
      pred_classID_false_pos = zeros(length(JIEDDO_class_ndx),1);
      pred_classID_accuracy = zeros(length(JIEDDO_class_ndx),1);
      i_JIEDDO_classID = 0;
      for i_classID = JIEDDO_class_ndx %%1 : pred_hdr.nf
	i_JIEDDO_classID = i_JIEDDO_classID + 1;
	pos_hist_tmp = squeeze(pred_classID_hist(:,i_JIEDDO_classID,1)) ./ squeeze(pred_classID_norm(:,i_JIEDDO_classID,1));
	neg_hist_tmp = squeeze(pred_classID_hist(:,i_JIEDDO_classID,2)) ./ squeeze(pred_classID_norm(:,i_JIEDDO_classID,2));
	if use_false_positive_thresh
	  pred_classID_bin_tmp = find( pred_classID_cumprob(:,i_JIEDDO_classID,2)>false_positive_thresh, 1, "first");
	else
	  pos_hist = pred_classID_hist(:,i_JIEDDO_classID,1)/pred_classID_sum(i_JIEDDO_classID,1);
	  neg_hist = pred_classID_hist(:,i_JIEDDO_classID,2)/(pred_classID_sum(i_JIEDDO_classID,2));
	  diff_hist = pos_hist - neg_hist;
	  pred_classID_bin_tmp = find((diff_hist>0).*(classID_hist_bins(:)>0), 1, "first");
	endif
	if ~isempty(pred_classID_bin_tmp)
	  pred_classID_thresh_bin(i_JIEDDO_classID) = pred_classID_bin_tmp;
	  pred_classID_thresh(i_JIEDDO_classID) = classID_hist_bins(pred_classID_bin_tmp);
	  pred_classID_true_pos(i_JIEDDO_classID) = (1 - pred_classID_cumprob(pred_classID_bin_tmp,i_JIEDDO_classID,1));
	  pred_classID_false_pos(i_JIEDDO_classID) = (pred_classID_cumprob(pred_classID_bin_tmp,i_JIEDDO_classID,2));
	  pred_classID_accuracy(i_JIEDDO_classID) = (pred_classID_true_pos(i_JIEDDO_classID) + pred_classID_false_pos(i_JIEDDO_classID)) / 2;
	else
	  pred_classID_thresh(i_JIEDDO_classID) = classID_hist_bins(end);
	pred_classID_true_pos(i_JIEDDO_classID) = 0.0;
	pred_classID_false_pos(i_JIEDDO_classID) = 0.0;
	pred_classID_accuracy(i_JIEDDO_classID) = 0.0;
       endif
     endfor  
     [pred_classID_val, pred_classID_ndx] = max(pred_classID_cube, [], 3);
     min_pred_classID = min(pred_classID_val(:))
     [max_pred_classID, max_pred_classID_ndx] = max(pred_classID_val(:))
     disp(classes{pred_classID_ndx(max_pred_classID_ndx)});
     mean_pred_classID = mean(pred_classID_val(:))
     std_pred_classID = std(pred_classID_val(:))
     pred_classID_thresh = reshape(pred_classID_thresh, [1,1, length(JIEDDO_class_ndx)]); %%gjk
     pred_classID_mask = double(JIEDDO_classID_cube >= repmat(pred_classID_thresh, [pred_hdr.ny, pred_hdr.nx, 1])); %% gjk
     pred_classID_confidences = cell(length(JIEDDO_class_ndx), 1);
     pred_classID_max_confidence = squeeze(max(squeeze(max(JIEDDO_classID_cube, [], 2)), [], 1));
     %% confidence is measured as a percentage relative to threshold
     %%   scaled by the residual hit rate relative to the false alarm rate
     pred_classID_max_percent_confidence = (pred_classID_max_confidence(:) - pred_classID_thresh(:)) ./ (pred_classID_thresh(:) + (pred_classID_thresh(:)==0));
     pred_classID_relative_accuracy = ((pred_classID_true_pos(:) - (1-false_positive_thresh)) ./ (1-pred_classID_false_pos(:)));
     pred_classID_max_confidence = pred_classID_max_percent_confidence;
     %%pred_classID_max_confidence = ((pred_classID_true_pos(:) - (1-pred_classID_false_pos(:))) ./ (1-pred_classID_false_pos(:))) .* (pred_classID_max_confidence(:) - pred_classID_thresh(:)) ./ (pred_classID_max_confidence(:) + pred_classID_thresh(:));
     [pred_classID_sorted_confidence, pred_classID_sorted_ndx] = sort(pred_classID_max_confidence, 'descend');
     JIEDDO_confidences = cell(length(JIEDDO_class_ndx),1);
     i_JIEDDO_classID = 0;
     for i_classID = JIEDDO_class_ndx %%1 : pred_hdr.nf
       i_JIEDDO_classID = i_JIEDDO_classID + 1;
       JIEDDO_confidences{i_JIEDDO_classID, 1} = [JIEDDO_classes{pred_classID_sorted_ndx(i_JIEDDO_classID)}, ...
						  ', ', num2str(pred_classID_sorted_confidence(i_JIEDDO_classID)), ...
						  ', ', num2str(pred_classID_thresh(pred_classID_sorted_ndx(i_JIEDDO_classID))), ...
						  ', ', num2str(pred_classID_accuracy(pred_classID_sorted_ndx(i_JIEDDO_classID))), ...
						  ', ', num2str(pred_classID_true_pos(pred_classID_sorted_ndx(i_JIEDDO_classID))), ...
						  ', ', num2str(pred_classID_false_pos(pred_classID_sorted_ndx(i_JIEDDO_classID)))];
     endfor
     
     if plot_flag
       pred_classID_heatmap = zeros(pred_hdr.ny, pred_hdr.nx, 3);
       pred_fig = figure("name", ["Predict: ", num2str(pred_time, "%i")]);
       image(uint8(pred_classID_heatmap)); axis off; axis image, box off;
       hold on
       i_JIEDDO_classID = 0;
       for i_classID = JIEDDO_class_ndx %% 1 : pred_hdr.nf
	 i_JIEDDO_classID = i_JIEDDO_classID + 1;
	 if ~any(pred_classID_mask(:,:,i_JIEDDO_classID))
	   continue;
	 endif
	 if i_JIEDDO_classID ~= pred_classID_sorted_ndx(1)
	   continue;
	 endif
	 pred_class_color = classID_colormap(i_JIEDDO_classID, :); 
	 pred_classID_band = repmat(pred_classID_mask(:,:,i_JIEDDO_classID), [1,1,3]);
	 pred_classID_band(:,:,1) = pred_classID_band(:,:,1) * pred_class_color(1)*255;
	 pred_classID_band(:,:,2) = pred_classID_band(:,:,2) * pred_class_color(2)*255;
	 pred_classID_band(:,:,3) = pred_classID_band(:,:,3) * pred_class_color(3)*255;
	 pred_classID_heatmap = pred_classID_heatmap + pred_classID_band .* (pred_classID_heatmap < pred_classID_band);
	 th = text(3, ceil(i_JIEDDO_classID*pred_hdr.ny/length(JIEDDO_class_ndx)), classes{i_classID});
	 pred_classID_heatmap(ceil(i_JIEDDO_classID*pred_hdr.ny/length(JIEDDO_class_ndx)):ceil(i_JIEDDO_classID*pred_hdr.ny/length(JIEDDO_class_ndx)), 1:2, 1) = pred_class_color(1)*255;
	 pred_classID_heatmap(ceil(i_JIEDDO_classID*pred_hdr.ny/length(JIEDDO_class_ndx)):ceil(i_JIEDDO_classID*pred_hdr.ny/length(JIEDDO_class_ndx)), 1:2, 2) = pred_class_color(2)*255;
	 pred_classID_heatmap(ceil(i_JIEDDO_classID*pred_hdr.ny/length(JIEDDO_class_ndx)):ceil(i_JIEDDO_classID*pred_hdr.ny/length(JIEDDO_class_ndx)), 1:2, 3) = pred_class_color(3)*255;
	 set(th, 'color', pred_class_color(:));
				%keyboard;
       endfor
       pred_classID_heatmap = mod(pred_classID_heatmap, 256);
       image(uint8(pred_classID_heatmap)); axis off; axis image, box off;
       drawnow
       %%get(th)
     endif %% plot_flag
     imwrite(uint8(pred_classID_heatmap), [output_dir, filesep, 'Recon', filesep, "pred_", num2str(pred_time, "%i"), "_", num2str(i_scale), '.png'], 'png');
     disp(JIEDDO_confidences)
				%disp([pred_classID_true_pos; pred_classID_false_pos])
				%keyboard
   endif
   
   
   %%gjk
   
   if(pred_time==Recon_time{1})
    
     disp("confusion matrix display")
     numeratorConfusion = zeros(gt_hdr.nf+1,length(JIEDDO_class_ndx)+1);
     denominatorConfusionAll = zeros(gt_hdr.nf+1,gt_hdr.nf+1); %% noident
     denominatorConfusion = zeros(gt_hdr.nf+1,length(JIEDDO_class_ndx)+1); %% noident
     numeratorConfusion = zeros(gt_hdr.nf+1,length(JIEDDO_class_ndx)+1); %%
     matrixConfusion = zeros(gt_hdr.nf,gt_hdr.nf);
     identifiedCar=0;
     for i_frame = 100 : min(pred_num_frames, gt_num_frames)
       i_frame;
       pred_data{i_frame}.time;
       gt_time = gt_data{i_frame}.time;                                    %% get ground truth first
       gt_num_active = length(gt_data{i_frame}.values);
       gt_active_ndx = gt_data{i_frame}.values+1;
       gt_active_sparse = sparse(gt_active_ndx,1,1,gt_num_neurons,1,gt_num_active);
       gt_classID_cube = full(gt_active_sparse);
       gt_classID_cube = reshape(gt_classID_cube, [gt_hdr.nf, gt_hdr.nx, gt_hdr.ny]);
       gt_classID_cube = permute(gt_classID_cube, [3,2,1]); %% gjk
       %% recon layer is not sparse
       pred_time = pred_data{i_frame}.time;
       pred_classID_cube = pred_data{i_frame}.values;
       pred_classID_cube = permute(pred_classID_cube, [2,1,3]);
       JIEDDO_classID_cube = pred_classID_cube(:,:,JIEDDO_class_ndx); %% there are only threshold for the JIEDDO
       pred_classID_mask = double(JIEDDO_classID_cube >= repmat(pred_classID_thresh*1.6, [pred_hdr.ny, pred_hdr.nx, 1])); %% gjkthres
       pred_classID_confidences = cell(length(JIEDDO_class_ndx), 1);
       pred_classID_max_confidence = squeeze(max(squeeze(max(JIEDDO_classID_cube, [], 2)), [], 1));
       %% confidence is measured as a percentage relative to threshold
       %% scaled by the residual hit rate relative to the false alarm rate
       pred_classID_max_percent_confidence = (pred_classID_max_confidence(:) - pred_classID_thresh(:)) ./ (pred_classID_thresh(:) + (pred_classID_thresh(:)==0));
       pred_classID_relative_accuracy = ((pred_classID_true_pos(:) - (1-false_positive_thresh)) ./ (1-pred_classID_false_pos(:)));
       pred_classID_max_confidence = pred_classID_max_percent_confidence;
       %%pred_classID_max_confidence = ((pred_classID_true_pos(:) - (1-pred_classID_false_pos(:))) ./ (1-pred_classID_false_pos(:))) .*\(pred_classID_maxr_confidence(:) - pred_classID_thresh(:)) ./ (pred_classID_max_confidence(:) + pred_classID_thresh(:));
       [pred_classID_sorted_confidence, pred_classID_sorted_ndx] = sort(pred_classID_max_confidence, 'descend');
       
       pred_category=pred_classID_sorted_ndx(1);      %% counting all tiles of the highest confidence class

       for xindex=1: gt_hdr.nx
	 for yindex=1: gt_hdr.ny

	   matrixConfusion = squeeze(gt_classID_cube(yindex,xindex,:)(:))*squeeze(~gt_classID_cube(yindex,xindex,:)(:))'; 
           matrixConfusion = matrixConfusion + diag(squeeze(gt_classID_cube(yindex,xindex,:)(:)));

           numeratorConfusion(2:gt_hdr.nf+1,2:length(JIEDDO_class_ndx)+1) = numeratorConfusion(2:gt_hdr.nf+1,2:length(JIEDDO_class_ndx)+1) + squeeze(matrixConfusion(:,JIEDDO_class_ndx)) .* repmat(squeeze(pred_classID_mask(yindex,xindex,:))',[gt_hdr.nf,1]);
           numeratorConfusion(1,1)    = numeratorConfusion(1,1)    +  ~any(gt_classID_cube(yindex,xindex,:))*~any(pred_classID_mask(yindex,xindex,:));
           numeratorConfusion(1,2:length(JIEDDO_class_ndx)+1)  = numeratorConfusion(1,2:length(JIEDDO_class_ndx)+1)  +  ~any(gt_classID_cube(yindex,xindex,:))*squeeze(pred_classID_mask(yindex,xindex,:)(:))';
           numeratorConfusion(2:gt_hdr.nf+1,1) = numeratorConfusion(2:gt_hdr.nf+1,1) +  ~any(pred_classID_mask(yindex,xindex,:))*squeeze(gt_classID_cube(yindex,xindex,:)(:));

           denominatorConfusionAll(2:gt_hdr.nf+1,2:gt_hdr.nf+1) = denominatorConfusionAll(2:gt_hdr.nf+1,2:gt_hdr.nf+1) + matrixConfusion;
           denominatorConfusionAll(2:gt_hdr.nf+1,1)    = denominatorConfusionAll(2:gt_hdr.nf+1,1)    + squeeze(gt_classID_cube(yindex,xindex,:)(:));
           denominatorConfusionAll(1,1:gt_hdr.nf+1)    = denominatorConfusionAll(1,1:gt_hdr.nf+1)    + ~any(gt_classID_cube(yindex,xindex,:));
         endfor
       endfor
     endfor  
     
    disp(pred_classID_file)
    printf('num %2i x %2i   noident   %s     %s     %s   %s  %s   %s \n',gt_hdr.nx, gt_hdr.ny,JIEDDO_classes{:}); 
    printf('    no ident %8i %8i %8i %8i %8i %8i %8i \n',numeratorConfusion(1,:));                 
           for iclasses=1:gt_hdr.nf
                printf('%12s %8i %8i %8i %8i %8i %8i %8i \n',classes{iclasses},numeratorConfusion(iclasses+1,:)); 
           endfor

    denominatorConfusion(:,2:length(JIEDDO_class_ndx)+1) = squeeze(denominatorConfusionAll(:,JIEDDO_class_ndx_Shift));
    denominatorConfusion(:,1) = denominatorConfusionAll(:,1);     
    disp(pred_classID_file)
    printf('den %2i x %2i   noident   %s     %s     %s   %s  %s   %s \n',gt_hdr.nx, gt_hdr.ny,JIEDDO_classes{:}); 
    printf('    no ident %8i %8i %8i %8i %8i %8i %8i \n',denominatorConfusion(1,:));  
	   for iclasses=1:gt_hdr.nf
                printf('%12s %8i %8i %8i %8i %8i %8i %8i \n',classes{iclasses},denominatorConfusion(iclasses+1,:));                       
           endfor

    ratioConfusion = double(numeratorConfusion)*100. ./ double(denominatorConfusion);

    errorConfusion = sqrt(1./double(numeratorConfusion) + 1./double(denominatorConfusion));

    errorConfusion = ratioConfusion .* errorConfusion;

    disp(pred_classID_file)
    printf('ratio%2ix%2i       noident   %s      %s        %s      %s    %s     %s \n',gt_hdr.nx, gt_hdr.ny,JIEDDO_classes{:});                    
    printf('    noident    ');
    for iprint=1:length(JIEDDO_class_ndx)+1
    printf(' %4.1f+-%3.1f ',ratioConfusion(1,iprint),errorConfusion(1,iprint));
    endfor  
    printf(' \n');
           for iclasses=1:gt_hdr.nf
           printf('%12s   ',classes{iclasses});
		  for iprint=1:length(JIEDDO_class_ndx)+1
                  if(iprint>1 && iclasses==JIEDDO_class_ndx(iprint-1))
                    printf("\033[46;30;1m");
                  endif
		  printf(' %4.1f+-%3.1f ',ratioConfusion(iclasses+1,iprint),errorConfusion(iclasses+1,iprint));                           if(iprint>1 && iclasses==JIEDDO_class_ndx(iprint-1))
                    printf("\033[0m");
                  endif
                  endfor
           printf(' \n');
           endfor

   endif

   %%printf("\033[46;30;1m hello world \033[0m\n");



   if plot_flag && i_frame == min(pred_num_frames, gt_num_frames)
      hist_fig = figure("name", ["hist_positive: ", num2str(i_scale), "_", num2str(pred_time, "%i")]);
      i_subplot = 0;
      i_JIEDDO_classID = 0;
      for i_classID  = JIEDDO_class_ndx %% 1 : pred_hdr.nf
	i_JIEDDO_classID = i_JIEDDO_classID + 1;
	i_subplot = i_subplot + 1;
	subplot(2,3,i_subplot, 'color', [0 0 0])
	%%
	pos_hist = squeeze(pred_classID_hist(:,i_JIEDDO_classID,1)) ./ squeeze(pred_classID_norm(:,i_JIEDDO_classID,1));
	hist_width_tmp = round(num_classID_bins/8);
	bins_tmp = [pred_classID_thresh_bin(i_JIEDDO_classID)-hist_width_tmp:pred_classID_thresh_bin(i_JIEDDO_classID)+hist_width_tmp];
	bins_tmp_fixed = bins_tmp(find(bins_tmp>0,1,"first"):find(bins_tmp<num_classID_bins,1,"last"));
	bh_pos = bar(classID_hist_bins(bins_tmp_fixed), pos_hist(bins_tmp_fixed), "stacked", "facecolor", "g", "edgecolor", "g");
	%%hist_fig = figure("name", ["hist_negative: ", num2str(pred_time, "%i")]);
	axis off
	 box off
	 hold on
	 neg_hist = squeeze(pred_classID_hist(:,i_JIEDDO_classID,2)) ./ squeeze(pred_classID_norm(:,i_JIEDDO_classID,2));
	 bh_neg = bar(classID_hist_bins(bins_tmp_fixed), neg_hist(bins_tmp_fixed), "stacked", "facecolor", "r", "edgecolor", "r");
	 max_pos_hist = max(pos_hist(:));
	 max_neg_hist = max(neg_hist(:));
	 lh = line([pred_classID_thresh(i_JIEDDO_classID) pred_classID_thresh(i_JIEDDO_classID)], [0 max(max_pos_hist,max_neg_hist)]);
	 set(lh, 'color', [0 0 1])
	 set(lh, 'linewidth', 1.0)
	 title(classes{i_classID});
      endfor
      saveas(hist_fig, [output_dir, filesep, 'Recon', filesep, "hist_", num2str(i_scale), "_", num2str(pred_time, "%i"), ".png"], "png");
    endif
    
  endfor
  save([output_dir, filesep, 'Recon', filesep, "hist_", num2str(pred_time, "%i"), ".mat"], "classID_hist_bins", "pred_classID_hist", "pred_classID_norm", "pred_classID_cumprob", "pred_classID_cumsum")
endfor  %% i_scale