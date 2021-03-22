#include "HeightFieldNode.h"
#include "Framework/Topology/HeightField.h"
#include "ShallowWaterEquationModel.h"
#include "IO/Image_IO/image.h"
#include "IO/Image_IO/image_io.h"

namespace PhysIKA
{
	IMPLEMENT_CLASS_1(HeightFieldNode, TDataType)

	template<typename TDataType>
	HeightFieldNode<TDataType>::HeightFieldNode(std::string name = "default")
		: Node(name)
	{
		auto swe = this->setNumericalModel<ShallowWaterEquationModel<TDataType>>("swe");
		this->SWEconnect();
		
		m_height_field = std::make_shared<HeightField<TDataType>>();
		this->setTopologyModule(m_height_field);
	}

	template<typename TDataType>
	void HeightFieldNode<TDataType>::SWEconnect()
	{
		auto swe = this->getModule<ShallowWaterEquationModel<TDataType>>("swe");
		this->currentPosition()->connect(&(swe->m_position));
		
		this->currentVelocity()->connect(&(swe->m_velocity));
		this->normal.connect(&(swe->normal));

		this->isBound.connect(&(swe->isBound));
		this->solid.connect(&(swe->solid));

		swe->setDistance(distance);
		swe->setRelax(relax);
		swe->setZcount(zcount);
	}

	template<typename TDataType>
	bool HeightFieldNode<TDataType>::initialize()
	{
		return Node::initialize();
	}

	__global__ void InitNeighbor(
		NeighborList<int> neighbors,
		int zcount,
		int xcount)
	{
		int i = threadIdx.x + (blockIdx.x * blockDim.x);
		if (i >= neighbors.size()) return;
		if(i%zcount==0)
			neighbors.setElement(i, 0, - 1);
		else
			neighbors.setElement(i, 0, i - 1);
		if((i+1)%zcount == 0)
			neighbors.setElement(i, 1, -1);
		else
			neighbors.setElement(i, 1, i + 1);

		neighbors.setElement(i, 2, i - zcount);
		neighbors.setElement(i, 3, i + zcount);
	}

	template<typename TDataType>
	void HeightFieldNode<TDataType>::loadHeightFieldParticles(Coord lo, Coord hi, int pixels, Real slope, std::vector<Coord>& vertList)
	{
		std::vector<Coord> normalList;
		std::vector<Coord> velList;
		Real height = 0, e = 2.71828;
		Real distance = (hi[2] - lo[2]) / (pixels - 1);
		Real xcenter = (hi[0] - lo[0]) / 2, zcenter = (hi[2] - lo[2]) / 2;
		
		Real x = lo[0];
		for (int i = 0; i < pixels; i++)
		{
			Real z = lo[2];
			for (int j = 0; j < pixels; j++)
			{
				height =  0.3 + slope * pow(e, -(pow(x - xcenter, 2) + pow(z - zcenter, 2)) * 100);
				Coord p = Coord(x, 0, z);
				vertList.push_back(Coord(x, height + lo[1], z));
				normalList.push_back(Coord(0, 1, 0));
				velList.push_back(Coord(0, 0, 0));
				z += distance;
			}
			x += distance;
		}

		this->currentVelocity()->setElementCount(velList.size());
		Function1Pt::copy(this->currentVelocity()->getValue(), velList);

		normalList.clear();
		velList.clear();

	}
	template<typename TDataType>
	void HeightFieldNode<TDataType>::loadHeightFieldFromImage(Coord lo, Coord hi, int pixels, Real slope, std::vector<Coord>& vertList)
	{
		std::vector<Coord> normalList;
		std::vector<Coord> velList;
		Real height = 0, e = 2.71828;
		Real distance = (hi[2] - lo[2]) / (pixels - 1);
		Real xcenter = (hi[0] - lo[0]) / 2, zcenter = (hi[2] - lo[2]) / 2;

		Real x = lo[0];
		for (int i = 0; i < pixels; i++)
		{
			Real z = lo[2];
			for (int j = 0; j < pixels; j++)
			{
				Coord p = Coord(x, 0, z);
				vertList.push_back(Coord(x, height + lo[1], z));
				normalList.push_back(Coord(0, 1, 0));
				velList.push_back(Coord(0, 0, 0));
				z += distance;
			}
			x += distance;
		}

		this->currentVelocity()->setElementCount(velList.size());
		Function1Pt::copy(this->currentVelocity()->getValue(), velList);

		normalList.clear();
		velList.clear();

	}
	template<typename TDataType>
	void HeightFieldNode<TDataType>::loadParticles(Coord lo, Coord hi, int pixels,Real slope, Real relax)
	{
		std::vector<Coord> vertList;
		loadHeightFieldParticles(lo, hi, pixels, slope, vertList);

		std::vector<Real> solidList;
		std::vector<Coord> normals;
		std::vector<int>  isbound;
		Real distance = (hi[2] - lo[2]) / (pixels-1);
		Real height =  0, e = 2.71828;
		Real xcenter = (hi[0] - lo[0]) / 2, zcenter = (hi[2] - lo[2]) / 2;
		Real x = lo[0];
		for (int i = 0; i < pixels; i++) 
		{
			Real z = lo[2];
			for (int j = 0;j < pixels; j++)
			{
				//height = 0.2+slope * pow(e, -(pow(x - xcenter, 2) + pow(z - zcenter, 2)) * 100);
				//height = z*0.45;
				if (z + distance > hi[2] || x + distance > hi[0] || x == lo[0] || z == lo[2])
					isbound.push_back(1);
				else
					isbound.push_back(0);
				//judge which one is higher
				if (lo[1] + height > vertList[j + i * pixels][1]) {
					vertList[j + i * pixels][1] = lo[1] + height;
				}
				solidList.push_back( lo[1] + height);
				normals.push_back(Coord(0, 1, 0));
				z += distance;
			}
			x += distance;
		}
		
		this->xcount = pixels;
		this->zcount = pixels;

		this->nx = xcount;
		this->nz = zcount;
		this->distance = distance;
		this->relax = relax;

		this->currentPosition()->setElementCount(vertList.size());
		Function1Pt::copy(this->currentPosition()->getValue(), vertList);

		solid.setElementCount(solidList.size());
		Function1Pt::copy(solid.getValue(), solidList);

		isBound.setElementCount(solidList.size());
		Function1Pt::copy(isBound.getValue(), isbound);

		normal.setElementCount(solidList.size());
		Function1Pt::copy(normal.getValue(), normals);

		this->SWEconnect();
		this->updateTopology();
		Coord ori = Coord(0, 0, 0);
		ori[2] = -0.5 * (lo[2] + hi[2]);
		ori[0] = -0.5 * (lo[0] + hi[0]);
		this->m_height_field->setOrigin(ori);

		printf("distance is %f ,zcount is %d, xcount is %d\n",distance, zcount, xcount);
		vertList.clear();
		solidList.clear();
		isbound.clear();
		normals.clear();
	}

	template<typename TDataType>
	void HeightFieldNode<TDataType>::loadParticlesFromImage( std::string filename1, std::string filename2, Real proportion, Real relax)
	{
		Image *image1 = new Image;
		Image *image2 = new Image;

 		ImageIO::load(filename1, image1);
		ImageIO::load(filename2, image2);
		assert(image2->height() == image2->width());
		assert(image2->width() == image1->height());
		assert(image1->height() == image1->width());
		int pixels = image1->height();
		this->distance = 0.003;
		Coord lo(0, 0, 0);
		Coord hi(distance * (pixels - 1), distance * (pixels - 1)*0.5, distance * (pixels - 1));

		std::vector<Coord> vertList;
		loadHeightFieldFromImage(lo, hi, pixels, 0, vertList);

		std::vector<Real> solidList;
		std::vector<Coord> normals;
		std::vector<int>  isbound;
		
		Real height = 0, e = 2.71828;
		Real xcenter = (hi[0] - lo[0]) / 2, zcenter = (hi[2] - lo[2]) / 2;
		Real x = lo[0];

		for (int i = 0; i < pixels; i++)
		{
			Real z = lo[2];
			for (int j = 0; j < pixels; j++)
			{
				if (z + distance > hi[2] || x + distance > hi[0] || x == lo[0] || z == lo[2])
					isbound.push_back(1);
				else
					isbound.push_back(0);
				//init terrain and river location
				int temp_index = (i*pixels + j)*image1->pixelSize();
				unsigned short temp_height = (image1->rawData()[temp_index + 1] << 8) | image1->rawData()[temp_index];
				height =  temp_height * proportion * (hi[1] - lo[1]) / 65535;
				if (image2->rawData()[temp_index] == 255) {
					vertList[j + i * pixels][1] = lo[1] + height;
					height = 0;//suppose the bottom of the river is 0
				}
				solidList.push_back(lo[1] + height);
				normals.push_back(Coord(0, 1, 0));
				z += distance;
				//judge which one is higher
				if (lo[1] + height > vertList[j + i * pixels][1]) {
					vertList[j + i * pixels][1] = solidList[j + i * pixels];
				}
			}
			x += distance;
		}
		this->xcount = pixels;
		this->zcount = pixels;

		this->nx = xcount;
		this->nz = zcount;
		this->distance = distance;
		this->relax = relax;

		this->currentPosition()->setElementCount(vertList.size());
		Function1Pt::copy(this->currentPosition()->getValue(), vertList);

		solid.setElementCount(solidList.size());
		Function1Pt::copy(solid.getValue(), solidList);

		isBound.setElementCount(solidList.size());
		Function1Pt::copy(isBound.getValue(), isbound);

		normal.setElementCount(solidList.size());
		Function1Pt::copy(normal.getValue(), normals);

		this->SWEconnect();
		this->updateTopology();
		this->init();

		printf("distance si %f ,zcount is %d, xcount is %d\n", distance, zcount, xcount);
		vertList.clear();
		solidList.clear();
		isbound.clear();
		normals.clear();

	}

	template<typename TDataType>
	std::vector<TDataType::Real>&  HeightFieldNode<TDataType>::outputSolid() {
		
		HostArrayField<Real> solidArrayField;
		int Size = this->solid.getValue().size();
		solidArrayField.setElementCount(Size);
		HostArray<Real> solidArray = solidArrayField.getValue();
		Function1Pt::copy(solidArray, this->solid.getValue());
		std::vector<Real> solid(solidArray.getDataPtr(), solidArray.getDataPtr() + Size);
		return solid;
	}

	template<typename TDataType>
	std::vector<TDataType::Real>& HeightFieldNode<TDataType>::outputDepth() {
		std::vector<Real> depth;
		HostArrayField<Real> solidArrayField;
		HostArrayField<Coord> heightArrayField;
		int Size = this->solid.getValue().size();
		heightArrayField.setElementCount(Size);
		solidArrayField.setElementCount(Size);
		auto solidArray = solidArrayField.getValue();
		auto heightArray = heightArrayField.getValue();
		Function1Pt::copy(heightArray, this->currentPosition()->getValue());
		Function1Pt::copy(solidArray, this->solid.getValue());
		for (int i = 0; i < Size; i++) {
			depth.push_back(heightArray[i][1] - solidArray[i]);
		}
		return depth;
	}

	template<typename TDataType>
	std::vector<TDataType::Real>&  HeightFieldNode<TDataType>::outputUVel() {
		
		std::vector<Real> Vel;
		HostArrayField<Coord> VelArrayField;
		int Size = this->solid.getValue().size();
		VelArrayField.setElementCount(Size);
		HostArray<Coord> VelArray = VelArrayField.getValue();
		Function1Pt::copy(VelArray, this->currentVelocity()->getValue());
		for (int i = 0; i < Size; i++) {
			Vel.push_back(VelArray[i][0]);
		}
		return Vel;
	}	
	
	template<typename TDataType>
	std::vector<TDataType::Real>&  HeightFieldNode<TDataType>::outputWVel() {
		
		std::vector<Real> Vel;
		HostArrayField<Coord> VelArrayField;
		int Size = this->solid.getValue().size();
		VelArrayField.setElementCount(Size);
		HostArray<Coord> VelArray = VelArrayField.getValue();
		Function1Pt::copy(VelArray, this->currentVelocity()->getValue());
		for (int i = 0; i < Size; i++) {
			Vel.push_back(VelArray[i][2]);
		}
		return Vel;
	}
	template<typename TDataType>
	void HeightFieldNode<TDataType>::init() {
		auto nModel = this->getNumericalModel();
		nModel->initialize();
	}

	template<typename TDataType>
	void HeightFieldNode<TDataType>::run(int stepNum, float timestep) {
		auto nModel = this->getNumericalModel();
		for (int i = 0; i < stepNum; i++) {
			nModel->step(timestep);
		}
	}

	template<typename TDataType>
	HeightFieldNode<TDataType>::~HeightFieldNode()
	{
	}
	template<typename TDataType>
	void HeightFieldNode<TDataType>::advance(Real dt)
	{
		auto nModel = this->getNumericalModel();
		nModel->step(dt);
	}

	template<typename Real, typename Coord>
	__global__ void SetupHeights(
		DeviceArray2D<Real> height, 
		DeviceArray2D<Real> terrain,
		DeviceArray<Coord> pts,
		DeviceArray<Real> solid)
	{
		int i = threadIdx.x + blockIdx.x * blockDim.x;
		int j = threadIdx.y + blockIdx.y * blockDim.y;

		if (i < height.Nx() && j < height.Ny())
		{
			int id = j + i * (height.Nx());
			height(i, j) = pts[id][1];
			terrain(i, j) = solid[id];
		}
	}

	template<typename TDataType>
	void HeightFieldNode<TDataType>::updateTopology()
	{
		if (!this->currentPosition()->isEmpty())
		{
			int num = this->currentPosition()->getElementCount();
			auto& pts = this->currentPosition()->getValue();
			m_height_field->setSpace(distance,distance);
			auto& heights = m_height_field->getHeights();
			auto& terrain = m_height_field->getTerrain();

			if (nx != heights.Nx() || nz != heights.Ny())
			{
				heights.resize(nx, nz);
				terrain.resize(nx, nz);
			}

			uint3 total_size;
			total_size.x = nx;
			total_size.y = nz;
			total_size.z = 1;

			cuExecute3D(total_size, SetupHeights,
				heights,
				terrain,
				pts,
				solid.getValue());
		}
	}

}