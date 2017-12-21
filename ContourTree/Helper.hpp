#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <ogr_geometry.h>

using std::string;
using std::vector;
using std::map;
using std::cout;
using std::endl;

typedef struct ContourNode {
	int id;
	int elev;
	vector<ContourNode*> *childs_higher;
	vector<ContourNode*> *childs_lower;
	ContourNode *father;
	bool flag;

	ContourNode(int p_fid, int p_elev) :id(p_fid), elev(p_elev), childs_higher(NULL), childs_lower(NULL), father(NULL), flag(false) {}
};

void get_real_filepath(int argc, char ** argv, string &input_path, string &output_path) {
	string input_string, output_string;
	if (argc > 1)
		input_string = string(argv[1]);
	if (argc > 2)
		output_string = string(argv[2]);

	string exe_path = string(argv[0]);

	size_t base_dir_pos = exe_path.find_last_of('\\');
	string base_path = exe_path.substr(0, base_dir_pos);


	size_t input_dir_pos = input_string.find_last_of('\\');
	if (input_dir_pos == -1)
		input_path = base_path + "\\" + input_string;
	else
		input_path = input_string;

	size_t output_dir_pos = output_string.find_last_of('\\');
	if (output_dir_pos == -1)
		output_path = base_path + "\\" + output_string;
	else
		output_path = output_string;
}

void build_relation(OGRLayer *layer, ContourNode *high_node, ContourNode *low_node) {
	OGRFeature *high = layer->GetFeature(high_node->id);
	OGRFeature *low = layer->GetFeature(low_node->id);
	OGRGeometry *high_polyline_geo = high->GetGeometryRef();
	OGRGeometry *high_polygon_geo = OGRGeometryFactory::forceToPolygon(high_polyline_geo);

	OGRGeometry *low_polyline_geo = low->GetGeometryRef();
	OGRGeometry *low_polygon_geo = OGRGeometryFactory::forceToPolygon(low_polyline_geo);

	if (high_polygon_geo->Within(low_polygon_geo)) {
		if (high_node->father != NULL) {
			OGRFeature *old_father = layer->GetFeature(high_node->father->id);
			OGRGeometry *old_father_polygon_geo = OGRGeometryFactory::forceToPolygon(old_father->GetGeometryRef());
			if (low_polygon_geo->Within(old_father_polygon_geo)) {
				auto tmp = high_node->father->childs_higher;
				for (vector<ContourNode*>::iterator node_iter = tmp->begin(); node_iter != tmp->end(); ) {
					if ((*node_iter)->id == high_node->id) {
						node_iter = tmp->erase(node_iter);
					}
					else
						++node_iter;
				}
				
				high_node->father = low_node;
			}
		}
		else {
			high_node->father = low_node;
		}

		if (low_node->childs_higher == NULL) {
			low_node->childs_higher = new vector<ContourNode*>();
			low_node->childs_higher->push_back(high_node);
		}
		else {
			low_node->childs_higher->push_back(high_node);
		}
	}

	else if (low_polygon_geo->Within(high_polygon_geo)) {
		if (low_node->father != NULL) {
			OGRFeature *old_father = layer->GetFeature(low_node->father->id);
			OGRGeometry *old_father_polygon_geo = OGRGeometryFactory::forceToPolygon(old_father->GetGeometryRef());
			if (high_polygon_geo->Within(old_father_polygon_geo)) {
				auto tmp = low_node->father->childs_lower;
				for (vector<ContourNode*>::iterator node_iter = tmp->begin(); node_iter != tmp->end(); ) {
					if ((*node_iter)->id == low_node->id) {
						node_iter = tmp->erase(node_iter);
					}
					else
						++node_iter;
				}

				low_node->father = high_node;
			}
		}
		else {
			low_node->father = high_node;
		}
		if (high_node->childs_lower == NULL) {
			high_node->childs_lower = new vector<ContourNode*>();
			high_node->childs_lower->push_back(low_node);
		}
		else {
			high_node->childs_lower->push_back(low_node);
		}
	}
}

void create_layer(map<int, vector<ContourNode*>*> contour_list, const string &output_path, OGRLayer *contour_layer) {
	GDALDriver *output_driver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
	if (output_driver == NULL) {
		cout << "driver not available." << endl;
		exit(1);
	}

	GDALDataset *output_dataset = output_driver->Create(output_path.c_str(), 0, 0, 0, GDT_Unknown, NULL);
	if (output_dataset == NULL) {
		cout << "creation of output file failed" << endl;
		exit(1);
	}

	OGRLayer *layer = output_dataset->CreateLayer("contour line", NULL, wkbMultiLineString, NULL);
	if (layer == NULL) {
		cout << "layer creation failed." << endl;
		exit(1);
	}

	OGRFieldDefn elev_field("elevation", OFTInteger);
	elev_field.SetWidth(32);

	OGRFieldDefn fid_field("ofid", OFTInteger64);
	fid_field.SetWidth(32);

	if (layer->CreateField(&elev_field) != OGRERR_NONE) {
		cout << "create elevation field failed." << endl;
		exit(1);
	}
	if (layer->CreateField(&fid_field) != OGRERR_NONE) {
		cout << "create elevation field failed." << endl;
		exit(1);
	}

	for (map<int, vector<ContourNode*>*>::iterator iter = contour_list.begin();
		iter != contour_list.end();
		++iter)
		for (auto node : *iter->second)
			if (node->childs_lower != NULL)
				if (node->childs_lower->size() >= 1) {
					if (node->flag == false) {
						node->flag = true;
						OGRFeature *o_feature = contour_layer->GetFeature(node->id);

						OGRFeature *feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
						feature->SetField("elevation", node->elev);
						feature->SetField("ofid", node->id);

						feature->SetGeometry(o_feature->GetGeometryRef());

						layer->CreateFeature(feature);
					}

					if (node->childs_lower->size() > 1) {
						for (auto c_node : *node->childs_lower) {
							if (c_node->flag == false) {
								c_node->flag = true;
								OGRFeature *o_feature = contour_layer->GetFeature(c_node->id);

								OGRFeature *feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
								feature->SetField("elevation", c_node->elev);
								feature->SetField("ofid", c_node->id);

								feature->SetGeometry(o_feature->GetGeometryRef());

								layer->CreateFeature(feature);
							}
						}
					}
				}

	GDALClose(output_dataset);
}

void build_contour_tree(const string &input_path, const string &output_path) {
	GDALAllRegister();

	cout << "open file......" << endl;
	GDALDataset *input_ds = (GDALDataset*)GDALOpenEx(input_path.c_str(), GDAL_OF_VECTOR, NULL, NULL, NULL);
	if (input_ds == NULL) {
		cout << "Open shapefile failed." << endl;
		exit(1);
	}
	cout << "open file completed." << endl;

	cout << "get layer......" << endl;
	OGRLayer *contour_layer = input_ds->GetLayer(0);
	OGRFeature *contour;
	contour_layer->ResetReading();
	cout << "get layer completed." << endl;

	map<int, vector<ContourNode*>*> contour_list;

	cout << "build contour list......" << endl;
	while ((contour = contour_layer->GetNextFeature()) != NULL) {
		size_t fid = contour->GetFID();
		int elev = contour->GetFieldAsInteger("elev");

		ContourNode *node = new ContourNode(fid, elev);

		if (contour_list.find(elev) == contour_list.end()) {
			vector<ContourNode*> *node_vector = new vector<ContourNode*>();
			node_vector->push_back(node);
			contour_list[elev] = node_vector;
		}
		else {
			contour_list[elev]->push_back(node);
		}
	}
	cout << "build contour list completed." << endl;

	cout << "build relation......" << endl;
	for (map<int, vector<ContourNode*>*>::iterator iter = (++contour_list.begin());
		iter != contour_list.end();
		++iter) {
		vector<ContourNode*> *high_level = iter->second;
		vector<ContourNode*> *low_level = (--iter)->second;
		++iter;

		for (auto high_node : *high_level) {
			for (auto low_node : *low_level) {
				build_relation(contour_layer, high_node, low_node);
			}
		}
	}
	cout << "build relation completed." << endl;

	cout << "save file......" << endl;
	create_layer(contour_list, output_path, contour_layer);
	cout << "save file completed." << endl;

	GDALClose(input_ds);
}