#include "BRTShaderInstancing2.h"
#include <osg/AlphaFunc>
#include <osg/Billboard>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Material>
#include <osg/Math>
#include <osg/MatrixTransform>
#include <osg/PolygonOffset>
#include <osg/Projection>
#include <osg/ShapeDrawable>
#include <osg/Texture2D>
#include <osg/TextureBuffer>
#include <osg/CullFace>
#include <osg/Image>
#include <osg/Texture2DArray>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/FileUtils>
#include "BillboardLayer.h"
#include "VegetationUtils.h"


namespace osgVegetation
{
	BRTShaderInstancing2::BRTShaderInstancing2(BillboardData &data) : m_PPL(false)
	{
		m_TrueBillboards = (data.Type == BT_ROTATED_QUAD);

		if(!(data.Type == BT_ROTATED_QUAD || data.Type == BT_CROSS_QUADS))
			OSGV_EXCEPT(std::string("BRTShaderInstancing2::BRTShaderInstancing2 - Unsupported billboard type").c_str());

		m_StateSet = _createStateSet(data);
	}

	BRTShaderInstancing2::~BRTShaderInstancing2()
	{

	}

	osg::StateSet* BRTShaderInstancing2::_createStateSet(BillboardData &data)
	{
		osg::ref_ptr<osg::Texture2DArray> tex = Utils::loadTextureArray(data);

		osg::StateSet *dstate = new osg::StateSet;
		dstate->setTextureAttribute(0, tex,	osg::StateAttribute::ON);

		osg::AlphaFunc* alphaFunc = new osg::AlphaFunc;
		alphaFunc->setFunction(osg::AlphaFunc::GEQUAL,data.AlphaRefValue);

		dstate->setAttributeAndModes( alphaFunc, osg::StateAttribute::ON );
		if(m_TrueBillboards)
			dstate->setAttributeAndModes(new osg::CullFace(),osg::StateAttribute::OFF);
		else
			dstate->setAttributeAndModes(new osg::CullFace(),osg::StateAttribute::ON);

		//dstate->setMode( GL_LIGHTING, osg::StateAttribute::OFF );

		if(data.UseAlphaBlend)
		{
			dstate->setAttributeAndModes( new osg::BlendFunc, osg::StateAttribute::ON );
			dstate->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
		}
		const int num_textures = tex->getNumImages();
		osg::Uniform* baseTextureSampler = new osg::Uniform(osg::Uniform::SAMPLER_2D_ARRAY, "baseTexture", num_textures);
		dstate->addUniform(baseTextureSampler);

		dstate->setMode( GL_LIGHTING, osg::StateAttribute::ON );

		{
			osg::Program* program = new osg::Program;
			//dstate->setAttribute(program);
			dstate->setAttributeAndModes( program, osg::StateAttribute::ON | osg::StateAttribute::PROTECTED );


			std::stringstream vertexShaderSource;
			vertexShaderSource <<
				//"#version 430 compatibility\n"
				"#extension GL_ARB_uniform_buffer_object : enable\n"

			"float IntNoise1( int x )"
			"{"
			"	int iSeed = 1000;"
			
			"	x = iSeed+x*57;"
			
			"	int n = x;"
			"	n =	(n<<13) ^ n;"
			"	return ( 1.0 - ( (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0);"    
			"}"

				"void DynamicShadow( in vec4 ecPosition )                               \n"
				"{                                                                      \n"
				"    // generate coords for shadow mapping                              \n"
				"    gl_TexCoord[2].s = dot( ecPosition, gl_EyePlaneS[2] );             \n"
				"    gl_TexCoord[2].t = dot( ecPosition, gl_EyePlaneT[2] );             \n"
				"    gl_TexCoord[2].p = dot( ecPosition, gl_EyePlaneR[2] );             \n"
				"    gl_TexCoord[2].q = dot( ecPosition, gl_EyePlaneQ[2] );             \n"
				"    gl_TexCoord[3].s = dot( ecPosition, gl_EyePlaneS[3] );             \n"
				"    gl_TexCoord[3].t = dot( ecPosition, gl_EyePlaneT[3] );             \n"
				"    gl_TexCoord[3].p = dot( ecPosition, gl_EyePlaneR[3] );             \n"
				"    gl_TexCoord[3].q = dot( ecPosition, gl_EyePlaneQ[3] );             \n"
				"} \n"
				"uniform samplerBuffer DataBufferTexture;\n"
				"uniform sampler2D	hmTexture;\n"
				"uniform float FadeInDist;\n"
				"varying vec2 TexCoord;\n";
			if(m_PPL)
			{
				vertexShaderSource <<
					"varying vec3 Normal;\n"
					"varying vec3 LightDir;\n";
			}
			vertexShaderSource <<
				"varying vec4 Color;\n"
				"varying vec3 Ambient;\n"
				"varying float VegetationType; \n"
				"const vec3 LightPosition = vec3(0.0, 0.0, 4.0);\n"
				"void main()\n"
				"{\n"
				"   vec3 normal;\n"
				"   int instanceAddress = gl_InstanceID * 3;\n"
				"   vec3 position = texelFetch(DataBufferTexture, instanceAddress).xyz;\n"
				"   Color         = texelFetch(DataBufferTexture, instanceAddress + 1);\n"
				"   vec4 data     = texelFetch(DataBufferTexture, instanceAddress + 2);\n"
				"   vec2 scale     = data.xy;\n"
				"   VegetationType = data.z;\n"
				"   vec4 camera_pos = gl_ModelViewMatrixInverse[3];\n"
				"   float len = 50.0;"
			    "   position.xy += camera_pos.xy;\n"
			    "   position.x = position.x - mod( camera_pos.x, len );"
				"   position.y = position.y - mod( camera_pos.y, len );"
				"float spacing = 1.0;"
				"float count = 50.0;"
				"float r = gl_InstanceID / count;"
				"float side_len = count*spacing;"
				// calc placement in ranges 0-1 for both x and y
				"position.x = ( fract( r ));"
			    "position.y = ((floor( r ) / count));"
				"position.x = mod( position.x+IntNoise1( gl_InstanceID ), 1.0 );"
				"position.y = mod( position.y+IntNoise1( gl_InstanceID ), 1.0 );"
				"vec4 thisCamPos = gl_ModelViewMatrixInverse*vec4(0,0,0,1);"

				"float xNorm = -mod( thisCamPos.x, side_len)/side_len;"
	      		"float yNorm = -mod( thisCamPos.y, side_len )/side_len;"
		    	"position.x = mod( position.x+xNorm, 1.0 );"
		    	"position.y = mod( position.y+yNorm, 1.0 );"
				

				"position.x = side_len * ( position.x- 0.5);"
				"position.y = side_len * ((position.y) - 0.5);"

			    "position.x = position.x + camera_pos.x;"
				"position.y = position.y + camera_pos.y;"
			    "position.z = texture2D(hmTexture, vec2( 0.1, 0.1)).r;";
				
				if(!data.CastShadows) //shadow casting and vertex fading don't mix well
			{
				vertexShaderSource <<
					"   float distance = length(camera_pos.xyz - position.xyz);\n";
					//"   scale = scale*clamp((1.0 - (distance-FadeInDist))/(FadeInDist*0.2),0.0,1.0);\n";
			}
			if(m_TrueBillboards)
			{
				vertexShaderSource <<
					"    vec3 dir = camera_pos.xyz - position.xyz;\n"
					"	 dir.z = 0;\n //we are only interested in xy-plane direction"
					"    dir = normalize(dir);\n"
					"    vec3 left = vec3(-dir.y,dir.x, 0);\n"
					"	 left = normalize(left);\n"
					"	 left.xy *= scale.xx;\n"
					"    vec4 m_pos = gl_Vertex;\n"
					"    m_pos.z *= scale.y;\n"
					"	 m_pos.xy = m_pos.x*left.xy;\n"
					"	 m_pos.xyz += position;\n";
				if(data.ReceiveShadows)
					vertexShaderSource << "   DynamicShadow(gl_ModelViewMatrix * m_pos);\n";
				vertexShaderSource << "   gl_Position = gl_ModelViewProjectionMatrix * m_pos;\n";
				//"   gl_Position = gl_ProjectionMatrix * modelView * gl_Vertex ;\n";
				if(data.TerrainNormal)
				{
					vertexShaderSource <<
						"   normal = normalize(gl_NormalMatrix * vec3(0,0,1));\n";
				}
				else
				{
					//skip standard normal transformation for billboards,
					//we want normal in eye-space and we know how to handle this transformation by hand
					vertexShaderSource <<
						"   normal = normalize(vec3(gl_Normal.x,0,-gl_Normal.y));\n";
				}
			}
			else
			{
				vertexShaderSource <<
					"   mat4 modelView = gl_ModelViewMatrix * mat4( scale.x, 0.0, 0.0, 0.0,\n"
					"              0.0, scale.x, 0.0, 0.0,\n"
					"              0.0, 0.0, scale.y, 0.0,\n"
					"              position.x, position.y, position.z, 1.0);\n"
					"   vec4 mv_pos = modelView * gl_Vertex;\n";
				if(data.ReceiveShadows)
					vertexShaderSource << "   DynamicShadow(mv_pos);\n";

				vertexShaderSource << "   gl_Position = gl_ProjectionMatrix * mv_pos ;\n";
				if(data.TerrainNormal)
				{
					vertexShaderSource <<
						"   normal = normalize(gl_NormalMatrix * vec3(0,0,1));\n";
				}
				else
				{
					vertexShaderSource <<
						//"   normal = normalize(gl_NormalMatrix * gl_Normal);\n";
						"   normal = normalize(vec3(gl_Normal.x,0,-gl_Normal.y));\n";
				}
			}

			if(m_PPL)
			{
				vertexShaderSource <<
					"   Normal = normal;\n"
					"   LightDir = normalize(gl_LightSource[0].position.xyz);\n"
					"   Ambient  = gl_LightSource[0].ambient.xyz;\n";
			}
			else
			{
				vertexShaderSource <<
					"   vec3 lightDir = normalize(gl_LightSource[0].position.xyz);\n"
					"   float NdotL = max(dot(normal, lightDir), 0.0);\n"
					"   Color.xyz = NdotL*Color.xyz*gl_LightSource[0].diffuse.xyz + gl_LightSource[0].ambient.xyz*Color.xyz;\n";
			}
			vertexShaderSource <<
				"   TexCoord = gl_MultiTexCoord0.st;\n"
				"}\n";

			std::stringstream fragmentShaderSource;
			fragmentShaderSource <<
				//"#version 430 core\n"
				"#extension GL_EXT_gpu_shader4 : enable\n"
				"#extension GL_EXT_texture_array : enable\n"
				"uniform sampler2DArray baseTexture; \n"

				"uniform sampler2DShadow shadowTexture0;                                 \n"
				"uniform int shadowTextureUnit0;                                         \n"
				"uniform sampler2DShadow shadowTexture1;                                 \n"
				"uniform int shadowTextureUnit1;                                         \n"

				"uniform float FadeInDist;\n"
				"varying float VegetationType; \n"
				"varying vec3 Ambient; \n"
				"varying vec2 TexCoord;\n";
			if(m_PPL)
			{
				fragmentShaderSource <<
					"varying vec3 Normal;\n"
					"varying vec3 LightDir;\n";
			}

			fragmentShaderSource <<
				"varying vec4 Color;\n"
				"void main(void) \n"
				"{\n"
				"    vec4 outColor = texture2DArray( baseTexture, vec3(TexCoord, VegetationType)); \n";
			if(m_PPL)
			{
				fragmentShaderSource <<
					"   float NdotL = max(dot(normalize(Normal), LightDir), 0.0);\n"
					"   outColor.xyz = NdotL*outColor.xyz*Color.xyz + Ambient.xyz*outColor.xyz*Color.xyz;\n";
			}
			else
			{
				fragmentShaderSource <<
					"   outColor.xyz = outColor.xyz * Color.xyz;\n";
			}

			if(data.ReceiveShadows)
			{
				fragmentShaderSource <<
					"  float shadow0 = shadow2DProj( shadowTexture0, gl_TexCoord[shadowTextureUnit0] ).r;   \n"
					"  float shadow1 = shadow2DProj( shadowTexture1, gl_TexCoord[shadowTextureUnit1] ).r;   \n"
					"  outColor.xyz = outColor.xyz *  shadow0*shadow1;                     \n";
			}
			fragmentShaderSource <<
				"    float depth = gl_FragCoord.z / gl_FragCoord.w;\n";
			if(data.UseFog)
			{
				switch(data.FogMode)
				{
				case osg::Fog::LINEAR:
					// Linear fog
					fragmentShaderSource << "float fogFactor = (gl_Fog.end - depth) * gl_Fog.scale;\n";
					break;
				case osg::Fog::EXP:
					// Exp fog
					fragmentShaderSource << "float fogFactor = exp(-gl_Fog.density * depth);\n";
					break;
				case osg::Fog::EXP2:
					// Exp fog
					fragmentShaderSource << "float fogFactor = exp(-pow((gl_Fog.density * depth), 2.0));\n";
					break;
				}
				fragmentShaderSource <<
					"fogFactor = clamp(fogFactor, 0.0, 1.0);\n"
					"outColor.xyz = mix(gl_Fog.color.xyz, outColor.xyz, fogFactor);\n";
			}
			fragmentShaderSource <<
				//"    finalColor.w = finalColor.w * clamp(1.0 - ((depth-FadeInDist)/(FadeInDist*0.1)), 0.0, 1.0);\n"
				"    gl_FragColor = outColor;\n"
				"}\n";


			std::stringstream ss_sufix;
			if(m_PPL) ss_sufix << "_ppl";
			if(m_TrueBillboards) ss_sufix << "_tbb";
			if(data.TerrainNormal)	ss_sufix << "_tn";
			if(data.ReceiveShadows) ss_sufix << "_s";
			ss_sufix << ".glsl";

			const std::string btr_vertex_file(std::string("btr_vertex" + ss_sufix.str()));
			const std::string btr_fragment_file(std::string("btr_fragment" + ss_sufix.str()));

			osg::Shader* vertex_shader = NULL;
			//first check if shader exist
			/*if(osgDB::fileExists(btr_vertex_file))
			{
			vertex_shader = new osg::Shader(osg::Shader::VERTEX);
			vertex_shader->setFileName(btr_vertex_file);
			vertex_shader->loadShaderSourceFromFile(btr_vertex_file);

			}
			else*/
			{
				vertex_shader = new osg::Shader(osg::Shader::VERTEX, vertexShaderSource.str());
				//Save shader
				//osgDB::writeShaderFile(*vertex_shader,btr_vertex_file);
			}
			program->addShader(vertex_shader);


			osg::Shader* fragment_shader = NULL;
			/*if(osgDB::fileExists(btr_fragment_file))
			{
			fragment_shader = new osg::Shader(osg::Shader::FRAGMENT);
			fragment_shader->setFileName(btr_fragment_file);
			fragment_shader->loadShaderSourceFromFile(btr_fragment_file);
			}
			else*/
			{
				fragment_shader = new osg::Shader(osg::Shader::FRAGMENT, fragmentShaderSource.str());
				//Save shader
				//osgDB::writeShaderFile(*fragment_shader,btr_fragment_file);
			}
			program->addShader(fragment_shader);
		}
		return dstate;
	}

	osg::Geometry* BRTShaderInstancing2::_createSingleQuadsWithNormals( const osg::Vec3& pos, float w, float h)
	{
		// set up the coords
		osg::Vec3Array& v = *(new osg::Vec3Array(8));
		osg::Vec3Array& n = *(new osg::Vec3Array(8));
		osg::Vec2Array& t = *(new osg::Vec2Array(8));

		float sw = w*0.5f;
		v[0].set(pos.x()-sw,pos.y(),pos.z()+0.0f);
		v[1].set(pos.x()   ,pos.y(),pos.z()+0.0f);
		v[2].set(pos.x()   ,pos.y(),pos.z()+h);
		v[3].set(pos.x()-sw,pos.y(),pos.z()+h);

		v[4].set(pos.x()   ,pos.y(),pos.z()+0.0f);
		v[5].set(pos.x()+sw,pos.y(),pos.z()+0.0f);
		v[6].set(pos.x()+sw,pos.y(),pos.z()+h);
		v[7].set(pos.x()   ,pos.y(),pos.z()+h);

		double roundness = 1.0;

		n[0].set(-roundness,-1,0);
		n[1].set( 0,-1,0);
		n[2].set( 0,-1,0);
		n[3].set(-roundness,-1,0);

		n[4].set( 0,-1.0,0);
		n[5].set( roundness,-1,0);
		n[6].set( roundness,-1,0);
		n[7].set( 0,-1,0);

		t[0].set(0.0f,0.0f);
		t[1].set(0.5f,0.0f);
		t[2].set(0.5f,1.0f);
		t[3].set(0.0f,1.0f);

		t[4].set(0.5f,0.0f);
		t[5].set(1.0f,0.0f);
		t[6].set(1.0f,1.0f);
		t[7].set(0.5f,1.0f);

		osg::Geometry *geom = new osg::Geometry;

		geom->setVertexArray( &v );
		geom->setNormalArray(&n);
		geom->setTexCoordArray( 0, &t );
		geom->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
		geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS,0,8));

		return geom;
	}


	osg::Geometry* BRTShaderInstancing2::_createOrthogonalQuadsWithNormals( const osg::Vec3& pos, float w, float h)
	{
		// set up the coords
		osg::Vec3Array& v = *(new osg::Vec3Array(16));
		osg::Vec3Array& n = *(new osg::Vec3Array(16));
		osg::Vec2Array& t = *(new osg::Vec2Array(16));

		float sw = w*0.5f;
		v[0].set(pos.x()-sw,pos.y(),pos.z()+0.0f);
		v[1].set(pos.x()+sw,pos.y(),pos.z()+0.0f);
		v[2].set(pos.x()+sw,pos.y(),pos.z()+h);
		v[3].set(pos.x()-sw,pos.y(),pos.z()+h);

		v[4].set(pos.x()-sw,pos.y(),pos.z()+h);
		v[5].set(pos.x()+sw,pos.y(),pos.z()+h);
		v[6].set(pos.x()+sw,pos.y(),pos.z()+0.0f);
		v[7].set(pos.x()-sw,pos.y(),pos.z()+0.0f);

		v[8].set(pos.x(),pos.y()+sw,pos.z()+0.0f);
		v[9].set(pos.x(),pos.y()-sw,pos.z()+0.0f);
		v[10].set(pos.x(),pos.y()-sw,pos.z()+h);
		v[11].set(pos.x(),pos.y()+sw,pos.z()+h);

		v[12].set(pos.x(),pos.y()+sw,pos.z()+h);
		v[13].set(pos.x(),pos.y()-sw,pos.z()+h);
		v[14].set(pos.x(),pos.y()-sw,pos.z()+0.0f);
		v[15].set(pos.x(),pos.y()+sw,pos.z()+0.0f);


		osg::Vec3 n1(0,-1,0);
		osg::Vec3 n2(0,1,0);

		osg::Vec3 n3(-1,0,0);
		osg::Vec3 n4(1,0,0);



		/*n[0]= n1;
		n[1]= n1;
		n[2]= n1;
		n[3]= n1;

		n[4]= n2;
		n[5]= n2;
		n[6]= n2;
		n[7]= n2;

		n[8]= n3;
		n[9]= n3;
		n[10]= n3;
		n[11]= n3;

		n[12]= n4;
		n[13]= n4;
		n[14]= n4;
		n[15]= n4;*/


		double roundness = 0.0;

		n[0].set(-roundness,-1,0);
		n[1].set( 0,-1,0);
		n[2].set( 0,-1,0);
		n[3].set(-roundness,-1,0);

		n[4].set( 0,-1.0,0);
		n[5].set( roundness,-1,0);
		n[6].set( roundness,-1,0);
		n[7].set( 0,-1,0);


		n[8].set(-roundness,-1,0);
		n[9].set( 0,-1,0);
		n[10].set( 0,-1,0);
		n[11].set(-roundness,-1,0);

		n[12].set( 0,-1.0,0);
		n[13].set( roundness,-1,0);
		n[14].set( roundness,-1,0);
		n[15].set( 0,-1,0);



		/*n[0].set(-roundness,-1,0);
		n[1].set( roundness,-1,0);
		n[2].set( roundness,-1,0);
		n[3].set(-roundness,-1,0);

		n[4].set(-roundness,-1,0);
		n[5].set( roundness,-1,0);
		n[6].set( roundness,-1,0);
		n[7].set(-roundness,-1,0);

		n[8].set(-roundness,-1,0);
		n[9].set( roundness,-1,0);
		n[10].set( roundness,-1,0);
		n[11].set(-roundness,-1,0);


		n[12].set(-roundness,-1,0);
		n[13].set( roundness,-1,0);
		n[14].set( roundness,-1,0);
		n[15].set(-roundness,-1,0);*/

		t[0].set(0.0f,0.0f);
		t[1].set(1.0f,0.0f);
		t[2].set(1.0f,1.0f);
		t[3].set(0.0f,1.0f);


		t[4].set(0.0f,1.0f);
		t[5].set(1.0f,1.0f);
		t[6].set(1.0f,0.0f);
		t[7].set(0.0f,0.0f);

		t[8].set(0.0f,0.0f);
		t[9].set(1.0f,0.0f);
		t[10].set(1.0f,1.0f);
		t[11].set(0.0f,1.0f);

		t[12].set(0.0f,1.0f);
		t[13].set(1.0f,1.0f);
		t[14].set(1.0f,0.0f);
		t[15].set(0.0f,0.0f);

		osg::Geometry *geom = new osg::Geometry;

		geom->setVertexArray( &v );
		geom->setNormalArray(&n);
		geom->setTexCoordArray( 0, &t );
		geom->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
		geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS,0,16));

		return geom;
	}

	osg::Node*  BRTShaderInstancing2::create(osg::ref_ptr<osg::Texture2D> hm_tex ,BillboardData &data, double view_dist, const osg::BoundingBoxd &bb)
	{
		BillboardVegetationObjectVector veg_objects;
		for(size_t i = 0; i < data.Layers.size(); i++)
		{
			_populateVegetationTile(data.Layers[i], bb, veg_objects);
		}
		return create(hm_tex,view_dist, veg_objects, bb);
	}

	void BRTShaderInstancing2::_populateVegetationTile(const BillboardLayer& layer,const  osg::BoundingBoxd& bb,BillboardVegetationObjectVector& instances) const
	{

		osg::Vec3d origin = bb._min; 
		osg::Vec3d size = bb._max - bb._min; 

		unsigned int num_objects_to_create = size.x()*size.y()*layer.Density;
		num_objects_to_create = 50*50;
		instances.reserve(instances.size()+num_objects_to_create);

		//std::cout << "pos:" << origin.x() << "size: " << size.x();
		for(unsigned int i=0;i<num_objects_to_create;++i)
		{
			double rand_x = Utils::random(origin.x(), origin.x() + size.x());
			double rand_y = Utils::random(origin.y(), origin.y() + size.y());
			osg::Vec3d pos(rand_x, rand_y,0);
			osg::Vec3d inter;
			osg::Vec4 terrain_color;
			osg::Vec4 coverage_color;
			float rand_int = Utils::random(layer.ColorIntensity.x(),layer.ColorIntensity.y());
						BillboardObject* veg_obj = new BillboardObject;
						float tree_scale = Utils::random(layer.Scale.x() ,layer.Scale.y());
						veg_obj->Width = Utils::random(layer.Width.x(), layer.Width.y())*tree_scale;
						veg_obj->Height = Utils::random(layer.Height.x(), layer.Height.y())*tree_scale;
				        veg_obj->TextureIndex = layer._TextureIndex;
				        veg_obj->Position = pos;
						veg_obj->Color = terrain_color*(layer.TerrainColorRatio*rand_int);
						veg_obj->Color += osg::Vec4(1,1,1,1)*(rand_int);
						veg_obj->Color.set(veg_obj->Color.r(), veg_obj->Color.g(), veg_obj->Color.b(), 1.0);
			instances.push_back(veg_obj);
			
		}
	}

	osg::Node* BRTShaderInstancing2::create(osg::ref_ptr<osg::Texture2D> hm_tex , double view_dist, const BillboardVegetationObjectVector &veg_objects, const osg::BoundingBoxd &bb)
	{
		osg::Geode* geode = 0;
		osg::Group* group = 0;
		if(veg_objects.size() > 0)
		{
			osg::ref_ptr<osg::Geometry> templateGeometry;
			if(m_TrueBillboards)
				templateGeometry = _createSingleQuadsWithNormals(osg::Vec3(0.0f,0.0f,0.0f),1.0f,1.0f);
			else
				templateGeometry = _createOrthogonalQuadsWithNormals(osg::Vec3(0.0f,0.0f,0.0f),1.0f,1.0f);

			templateGeometry->setUseVertexBufferObjects(true);
			templateGeometry->setUseDisplayList(false);
			osg::Geometry* geometry = (osg::Geometry*)templateGeometry->clone( osg::CopyOp::DEEP_COPY_PRIMITIVES );
			geometry->setUseDisplayList(false);
			osg::DrawArrays* primSet = dynamic_cast<osg::DrawArrays*>( geometry->getPrimitiveSet(0) );
			primSet->setNumInstances( veg_objects.size() );
			geode = new osg::Geode;
			geode->addDrawable(geometry);
			unsigned int i=0;
			osg::ref_ptr<osg::Image> treeParamsImage = new osg::Image;
			treeParamsImage->allocateImage( 3*veg_objects.size(), 1, 1, GL_RGBA, GL_FLOAT );
			for(BillboardVegetationObjectVector::const_iterator itr= veg_objects.begin();
				itr!= veg_objects.end();
				++itr,++i)
			{
				osg::Vec4f* ptr = (osg::Vec4f*)treeParamsImage->data(3*i);
				BillboardObject& tree = **itr;
				ptr[0] = osg::Vec4f(tree.Position.x(),tree.Position.y(),tree.Position.z(),1.0);
				ptr[1] = osg::Vec4f((float)tree.Color.r(),(float)tree.Color.g(), (float)tree.Color.b(), 1.0);
				ptr[2] = osg::Vec4f(tree.Width, tree.Height, tree.TextureIndex, 1.0);
			}

			osg::ref_ptr<osg::TextureBuffer> tbo = new osg::TextureBuffer;
			tbo->setImage( treeParamsImage.get() );
			tbo->setInternalFormat(GL_RGBA32F_ARB);
			geometry->getOrCreateStateSet()->setTextureAttribute(1, tbo.get(),osg::StateAttribute::ON);
			osg::BoundingBox infinite_bb;
			infinite_bb._min.set(-1000,-1000,-1000);
			infinite_bb._max.set(1000,1000,1000);
			geometry->setInitialBound( infinite_bb );
			osg::Uniform* dataBufferSampler = new osg::Uniform("DataBufferTexture",1);
			geometry->getOrCreateStateSet()->addUniform(dataBufferSampler);

			osg::Uniform* fadeInDist = new osg::Uniform(osg::Uniform::FLOAT, "FadeInDist");

			float f_vd = view_dist;
			fadeInDist->set(f_vd);
			geometry->getOrCreateStateSet()->addUniform(fadeInDist);

			//osg::Uniform* hmTextureSampler = new osg::Uniform(osg::Uniform::SAMPLER_2D, "hmTexture",1);
			osg::Uniform* hmTextureSampler = new osg::Uniform("hmTexture",2);

			geometry->getOrCreateStateSet()->addUniform(hmTextureSampler);

			

		}
		m_StateSet->setTextureAttribute(2, hm_tex.get(),	osg::StateAttribute::ON);
		geode->setStateSet(m_StateSet);
		return geode;
	}
}
