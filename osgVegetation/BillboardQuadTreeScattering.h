#pragma once
#include "Common.h"
#include <osg/BoundingBox>
#include <osg/Referenced>
#include <osg/Node>
#include <osg/ref_ptr>

#include <vector>
#include "IBillboardRenderingTech.h"
#include "BillboardLayer.h"
#include "BillboardData.h"
#include "EnvironmentSettings.h"

namespace osgVegetation
{
	class ITerrainQuery;

	/**
		Class used for billboard generation. Billboards are stored in quad tree
		structure based on the osg::LOD node. The start tile use the supplied
		bounding box which is then recursively divided into four new tiles until
		the cutoff tile size is reached. The cutoff tile size i calculated based
		on the shortest vegetation layer distance.
	*/

	class osgvExport BillboardQuadTreeScattering : public osg::Referenced
	{
	public:
		/**
		@param tq Pointer to TerrainQuery class, used during the scattering step.
		*/
		BillboardQuadTreeScattering(ITerrainQuery* tq, const EnvironmentSettings& env_settings);
		/**
			Generate vegetation data by providing billboard data
			@param bb Generation area
			@param data Billboard layers and settings
			@param out_put_file Filename if you want to save data base, if using paged LODs this also will decide format and path for all Paged LOD files
			@param use_paged_lod Use PagedLOD instead of regular LOD nodes (can only be true if out_put_file is defined, otherwise we have no path)
			@param filename_prefix Added to all files (only relevant if out_put_file is defined)
		*/
		osg::Node* generate(const osg::BoundingBoxd &bb, BillboardData &data, const std::string &output_file = "", bool use_paged_lod = false, const std::string &filename_prefix = "");

		osg::Node* generate(const osg::BoundingBoxd &bb,std::vector<osgVegetation::BillboardData> &data, const std::string &output_file, bool use_paged_lod);
	private:
		int m_FinalLOD;

		//data used for progress report
		int m_CurrentTile;
		int m_NumberOfTiles;

		//Area bounding box
		osg::BoundingBoxd m_InitBB;
		
		IBillboardRenderingTech* m_BRT;
		
		//Offset based on initial bounding box, used to avoid floating point precision problem 
		osg::Vec3d m_Offset; 
	
		ITerrainQuery* m_TerrainQuery;
		EnvironmentSettings m_EnvironmentSettings;
		bool m_UsePagedLOD;

		//Output stuff
		std::string m_SavePath;
		std::string m_FilenamePrefix;
		std::string m_SaveExt;

		//Helpers
		std::string _createFileName(unsigned int lv,	unsigned int x, unsigned int y) const;
		void _populateVegetationTile(const BillboardLayer& layer,const osg::BoundingBoxd &box, BillboardVegetationObjectVector& instances, osg::BoundingBoxd& out_bb) const;
		osg::Node* _createLODRec(int ld, BillboardData &data, BillboardVegetationObjectVector trees, const osg::BoundingBoxd &box ,int x, int y);
	};
}
