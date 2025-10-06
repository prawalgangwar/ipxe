#ifndef _GVE_H
#define _GVE_H

/** @file
 *
 * Google Virtual Ethernet network driver
 *
 * The Google Virtual Ethernet NIC (GVE or gVNIC) is found only in
 * Google Cloud instances.  There is essentially zero documentation
 * available beyond the mostly uncommented source code in the Linux
 * kernel.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/dma.h>
#include <ipxe/pci.h>
#include <ipxe/in.h>
#include <ipxe/process.h>
#include <ipxe/retry.h>

struct gve_nic;

/**
 * A Google Cloud MAC address
 *
 * Google Cloud locally assigned MAC addresses encode the local IPv4
 * address in the trailing 32 bits, presumably as a performance
 * optimisation to allow ARP resolution to be skipped by a suitably
 * aware network stack.
 */
struct google_mac {
	/** Reserved */
	uint8_t reserved[2];
	/** Local IPv4 address */
	struct in_addr in;
} __attribute__ (( packed ));

/** Page size */
#define GVE_PAGE_SIZE 0x1000

/**
 * Address alignment
 *
 * All DMA data structure base addresses seem to need to be aligned to
 * a page boundary.  (This is not documented anywhere, but is inferred
 * from existing source code and experimentation.)
 */
#define GVE_ALIGN GVE_PAGE_SIZE

/** Configuration BAR */
#define GVE_CFG_BAR PCI_BASE_ADDRESS_0

/**
 * Configuration BAR size
 *
 * All registers within the configuration BAR are big-endian.
 */
#define GVE_CFG_SIZE 0x1000

/** Device status */
#define GVE_CFG_DEVSTAT 0x0000
#define GVE_CFG_DEVSTAT_RESET 0x00000010UL	/**< Device is reset */

/** Device status flags */
enum gve_device_status_flags {
	GVE_DEVICE_STATUS_RESET_MASK		= 0x2,
	GVE_DEVICE_STATUS_LINK_STATUS_MASK	= 0x4,
	GVE_DEVICE_STATUS_REPORT_STATS_MASK	= 0x8,
	GVE_DEVICE_STATUS_DEVICE_IS_RESET	= 0x10,
};

/** Driver status */
#define GVE_CFG_DRVSTAT 0x0004
#define GVE_CFG_DRVSTAT_RUN 0x00000001UL	/**< Run admin queue */

/** Maximum time to wait for reset */
#define GVE_RESET_MAX_WAIT_MS 500

/** Driver status flags */
enum gve_driver_status_flags {
	GVE_DRIVER_STATUS_RESET_MASK		= 0x2,
};

/** Admin queue page frame number (for older devices) */
#define GVE_CFG_ADMIN_PFN 0x0010

/** Admin queue doorbell */
#define GVE_CFG_ADMIN_DB 0x0014

/** Admin queue event counter */
#define GVE_CFG_ADMIN_EVT 0x0018

/** Driver version (8-bit register) */
#define GVE_CFG_VERSION 0x001f

/** Admin queue base address high 32 bits */
#define GVE_CFG_ADMIN_BASE_HI 0x0020

/** Admin queue base address low 32 bits */
#define GVE_CFG_ADMIN_BASE_LO 0x0024

/** Admin queue base address length (16-bit register) */
#define GVE_CFG_ADMIN_LEN 0x0028

/** Doorbell BAR */
#define GVE_DB_BAR PCI_BASE_ADDRESS_2

/**
 * Admin queue entry header
 *
 * All values within admin queue entries are big-endian.
 */
struct gve_admin_header {
	/** Reserved */
	uint8_t reserved[3];
	/** Operation code */
	uint8_t opcode;
	/** Status */
	uint32_t status;
} __attribute__ (( packed ));

/** Command succeeded */
#define GVE_ADMIN_STATUS_OK 0x00000001

/** Simple admin command */
struct gve_admin_simple {
	/** Header */
	struct gve_admin_header hdr;
	/** ID */
	uint32_t id;
} __attribute__ (( packed ));

/** Describe device command */
#define GVE_ADMIN_DESCRIBE 0x0001

/** Describe device command */
struct gve_admin_describe {
	/** Header */
	struct gve_admin_header hdr;
	/** Descriptor buffer address */
	uint64_t addr;
	/** Descriptor version */
	uint32_t ver;
	/** Descriptor maximum length */
	uint32_t len;
} __attribute__ (( packed ));

/** Device descriptor version */
#define GVE_ADMIN_DESCRIBE_VER 1

/** Device descriptor */
struct gve_device_descriptor {
	/** Maximum number of pages that can be registered */
	uint64_t max_registered_pages;
	/** Reserved */
	uint16_t reserved1;
	/** Number of transmit queue entries */
	uint16_t tx_count;
	/** Number of receive queue entries */
	uint16_t rx_count;
	/** Default number of queues */
	uint16_t default_num_queues;
	/** Maximum transmit unit */
	uint16_t mtu;
	/** Number of event counters */
	uint16_t counters;
	/** Transmit pages per QPL */
	uint16_t tx_pages_per_qpl;
	/** Receive pages per QPL */
	uint16_t rx_pages_per_qpl;
	/** MAC address */
	struct google_mac mac;
	/** Number of device options */
	uint16_t num_device_options;
	/** Total length of descriptor */
	uint16_t total_length;
	/** Reserved */
	uint8_t reserved3[6];
} __attribute__ (( packed ));

/** Device option */
struct gve_device_option {
	/** Option ID */
	uint16_t option_id;
	/** Option length */
	uint16_t option_length;
	/** Required features mask */
	uint32_t required_features_mask;
} __attribute__ (( packed ));

/** Configure device resources command */
#define GVE_ADMIN_CONFIGURE 0x0002

/** Configure device resources command */
struct gve_admin_configure {
	/** Header */
	struct gve_admin_header hdr;
	/** Event counter array */
	uint64_t events;
	/** IRQ doorbell address */
	uint64_t irqs;
	/** Number of event counters */
	uint32_t num_events;
	/** Number of IRQ doorbells */
	uint32_t num_irqs;
	/** IRQ doorbell stride */
	uint32_t irq_stride;
	/** MSI-X base index for notification blocks */
	uint32_t ntfy_blk_msix_base_idx;
	/** Queue format */
	uint8_t queue_format;
	/** Padding */
	uint8_t padding[7];
} __attribute__ (( packed ));

/** Register page list command */
#define GVE_ADMIN_REGISTER 0x0003

/** Register page list command */
struct gve_admin_register {
	/** Header */
	struct gve_admin_header hdr;
	/** Page list ID */
	uint32_t id;
	/** Number of pages */
	uint32_t count;
	/** Address list address */
	uint64_t addr;
	/** Page size */
	uint64_t size;
} __attribute__ (( packed ));

/**
 * Maximum number of pages per queue
 *
 * This is a policy decision.  Must be sufficient to allow for both
 * the transmit and receive queue fill levels.
 */
#define GVE_QPL_MAX 64

/** Page list */
struct gve_pages {
	/** Page address */
	uint64_t addr[GVE_QPL_MAX];
} __attribute__ (( packed ));

/** Unregister page list command */
#define GVE_ADMIN_UNREGISTER 0x0004

/** Create transmit queue command */
#define GVE_ADMIN_CREATE_TX 0x0005

/** Create transmit queue command */
struct gve_admin_create_tx {
	/** Header */
	struct gve_admin_header hdr;
	/** Queue ID */
	uint32_t id;
	/** Reserved */
	uint8_t reserved_a[4];
	/** Queue resources address */
	uint64_t res;
	/** Descriptor ring address */
	uint64_t desc;
	/** Queue page list ID */
	uint32_t qpl_id;
	/** Notification channel ID */
	uint32_t notify_id;
	/** Transmit completion ring address */
	uint64_t comp_ring_addr;
	/** Transmit ring size */
	uint16_t ring_size;
	/** Transmit completion ring size */
	uint16_t comp_ring_size;
	/** Padding */
	uint8_t padding[4];
} __attribute__ (( packed ));

/** Create receive queue command */
#define GVE_ADMIN_CREATE_RX 0x0006

/** Create receive queue command */
struct gve_admin_create_rx {
	/** Header */
	struct gve_admin_header hdr;
	/** Queue ID */
	uint32_t id;
	/** Index */
	uint32_t index;
	/** Reserved */
	uint8_t reserved_a[4];
	/** Notification channel ID */
	uint32_t notify_id;
	/** Queue resources address */
	uint64_t res;
	/** Completion ring address, rx_desc_ring_addr, information about the final descriptor*/ 
	uint64_t cmplt;
	/** Descriptor ring address, rx_data_ring_addr, buffer address list */
	uint64_t desc;
	/** Queue page list ID */
	uint32_t qpl_id;
	/** Rx ring size */
	uint16_t rx_ring_size;
	/** Packet buffer size */
	uint16_t bufsz;
	/** Receive buffer ring size */
	uint16_t rx_buff_ring_size;
	/** Enable RSC */
	uint8_t enable_rsc;
	/** Padding */
	uint8_t padding1;
	/** Header buffer size */
	uint16_t header_buffer_size;
	/** Padding */
	uint8_t padding2[2];
} __attribute__ (( packed ));

/** Destroy transmit queue command */
#define GVE_ADMIN_DESTROY_TX 0x0007

/** Destroy receive queue command */
#define GVE_ADMIN_DESTROY_RX 0x0008

/** Deconfigure device resources command */
#define GVE_ADMIN_DECONFIGURE 0x0009

/** An admin queue command */
union gve_admin_command {
	/** Header */
	struct gve_admin_header hdr;
	/** Simple command */
	struct gve_admin_simple simple;
	/** Describe device */
	struct gve_admin_describe desc;
	/** Configure device resources */
	struct gve_admin_configure conf;
	/** Register page list */
	struct gve_admin_register reg;
	/** Create transmit queue */
	struct gve_admin_create_tx create_tx;
	/** Create receive queue */
	struct gve_admin_create_rx create_rx;
	/** Padding */
	uint8_t pad[64];
};

/* GVE_QUEUE_FORMAT_UNSPECIFIED must be zero since 0 is the default value
 * when the entire configure_device_resources command is zeroed out and the
 * queue_format is not specified.
 */
enum gve_queue_format {
	GVE_QUEUE_FORMAT_UNSPECIFIED	= 0x0,
	GVE_GQI_QPL_FORMAT		= 0x2,
	GVE_DQO_RDA_FORMAT		= 0x3,
	GVE_DQO_QPL_FORMAT		= 0x4,
};

/** Raw addressing QPL ID */
#define GVE_RAW_ADDRESSING_QPL_ID 0xFFFFFFFF

/**
 * Number of admin queue commands
 *
 * This is theoretically a policy decision.  However, older revisions
 * of the hardware seem to have only the "admin queue page frame
 * number" register and no "admin queue length" register, with the
 * implication that the admin queue must be exactly one page in
 * length.
 *
 * Choose to use a one page (4kB) admin queue for both older and newer
 * versions of the hardware, to minimise variability.
 */
#define GVE_ADMIN_COUNT ( GVE_PAGE_SIZE / sizeof ( union gve_admin_command ) )

/** Admin queue */
struct gve_admin {
	/** Commands */
	union gve_admin_command *cmd;
	/** Producer counter */
	uint32_t prod;
	/** DMA mapping */
	struct dma_mapping map;
};

/** Scratch buffer for admin queue commands */
struct gve_scratch {
	/** Buffer contents */
	union {
		/** Device descriptor */
		struct gve_device_descriptor desc;
		/** Page address list */
		struct gve_pages pages;
		/** Raw data */
		uint8_t raw[GVE_PAGE_SIZE];
	} *buf;
	/** DMA mapping */
	struct dma_mapping map;
};

/**
 * An event counter
 *
 * Written by the device to indicate completions.  The device chooses
 * which counter to use for each transmit queue, and stores the index
 * of the chosen counter in the queue resources.
 */
struct gve_event {
	/** Number of events that have occurred */
	volatile uint32_t count;
} __attribute__ (( packed ));

/** Event counter array */
struct gve_events {
	/** Event counters */
	struct gve_event *event;
	/** DMA mapping */
	struct dma_mapping map;
	/** Actual number of event counters */
	unsigned int count;
};

/** An interrupt channel */
struct gve_irq {
	/** Interrupt doorbell index (within doorbell BAR) */
	uint32_t db_idx;
	/** Reserved */
	uint8_t reserved[60];
} __attribute__ (( packed ));

/**
 * Number of interrupt channels
 *
 * We tell the device how many interrupt channels we have provided via
 * the "configure device resources" admin queue command.  The device
 * will accept being given zero interrupt channels, but will
 * subsequently fail to create more than a single queue (either
 * transmit or receive).
 *
 * There is, of course, no documentation indicating how may interrupt
 * channels actually need to be provided.  In the absence of evidence
 * to the contrary, assume that two channels (one for transmit, one
 * for receive) will be sufficient.
 */
#define GVE_IRQ_COUNT 2

/** Interrupt channel array */
struct gve_irqs {
	/** Interrupt channels */
	struct gve_irq *irq;
	/** DMA mapping */
	struct dma_mapping map;
	/** Interrupt doorbells */
	volatile uint32_t *db[GVE_IRQ_COUNT];
};

/** Disable interrupts */
#define GVE_IRQ_DISABLE 0x40000000UL

/**
 * Queue resources
 *
 * Written by the device to indicate the indices of the chosen event
 * counter and descriptor doorbell register.
 *
 * This appears to be a largely pointless data structure: the relevant
 * information is static for the lifetime of the queue and could
 * trivially have been returned in the response for the "create
 * transmit/receive queue" command, instead of requiring yet another
 * page-aligned coherent DMA buffer allocation.
 */
struct gve_resources {
	/** Descriptor doorbell index (within doorbell BAR) */
	uint32_t db_idx;
	/** Event counter index (within event counter array) */
	uint32_t evt_idx;
	/** Reserved */
	uint8_t reserved[56];
} __attribute__ (( packed ));

/**
 * Queue data buffer size
 *
 * In theory, we may specify the size of receive buffers.  However,
 * the original version of the device seems not to have a parameter
 * for this, and assumes the use of half-page (2kB) buffers.  Choose
 * to use this as the buffer size, on the assumption that older
 * devices will not support any other buffer size.
 */
#define GVE_BUF_SIZE ( GVE_PAGE_SIZE / 2 )

/** Number of data buffers per page */
#define GVE_BUF_PER_PAGE ( GVE_PAGE_SIZE / GVE_BUF_SIZE )

/**
 * Queue page list
 *
 * The device uses preregistered pages for fast-path DMA operations
 * (i.e. transmit and receive buffers).  A list of device addresses
 * for each page must be registered before the transmit or receive
 * queue is created, and cannot subsequently be modified.
 *
 * The Linux driver allocates pages as DMA_TO_DEVICE or
 * DMA_FROM_DEVICE as appropriate, and uses dma_sync_single_for_cpu()
 * etc to ensure that data is copied to/from bounce buffers as needed.
 *
 * Unfortunately there is no such sync operation available within our
 * DMA API, since we are constrained by the limitations imposed by
 * EFI_PCI_IO_PROTOCOL.  There is no way to synchronise a buffer
 * without also [un]mapping it, and no way to force the reuse of the
 * same device address for a subsequent remapping.  We are therefore
 * constrained to use only DMA-coherent buffers, since this is the
 * only way we can repeatedly reuse the same device address.
 *
 * Newer versions of the gVNIC device support "raw DMA addressing
 * (RDA)", which is essentially a prebuilt queue page list covering
 * the whole of the guest address space.  Unfortunately we cannot rely
 * on this, since older versions will not support it.
 *
 * Experimentation suggests that the device will accept a request to
 * create a queue page list covering the whole of the guest address
 * space via two giant "pages" of 2^63 bytes each.  However,
 * experimentation also suggests that the device will accept any old
 * garbage value as the "page size".  In the total absence of any
 * documentation, it is probably unsafe to conclude that the device is
 * bothering to look at or respect the "page size" parameter: it is
 * most likely just presuming the use of 4kB pages.
 */
struct gve_qpl {
	/** Page addresses */
	void *data;
	/** Page mapping */
	struct dma_mapping map;
	/** Number of pages */
	unsigned int count;
	/** Queue page list ID */
	unsigned int id;
};

struct gve_packet {
	/** Page addresses */
	void *data;
	/** Page mapping */
	struct dma_mapping map;
	/** Number of pages */
	unsigned int count;
};

/**
 * Maximum number of transmit buffers
 *
 * This is a policy decision.
 */
#define GVE_TX_FILL 16

/** Transmit queue page list ID */
#define GVE_TX_QPL 0x18ae5458

/** Tranmsit queue interrupt channel */
#define GVE_TX_IRQ 0

/** A transmit or receive buffer descriptor */
struct gve_buffer {
	/** Address (within queue page list address space) */
	uint64_t addr;
} __attribute__ (( packed ));

/** A transmit packet descriptor */
struct gve_tx_packet {
	/** Type */
	uint8_t type;
	/** Reserved */
	uint8_t reserved_a[2];
	/** Number of descriptors in this packet */
	uint8_t count;
	/** Total length of this packet */
	uint16_t total;
	/** Length of this descriptor */
	uint16_t len;
} __attribute__ (( packed ));

/** A transmit descriptor */
struct gve_tx_descriptor_gqi {
	/** Packet descriptor */
	struct gve_tx_packet pkt;
	/** Buffer descriptor */
	struct gve_buffer buf;
} __attribute__ (( packed ));

/** Start of packet transmit descriptor type */
#define GVE_TX_TYPE_START 0x00

/** Continuation of packet transmit descriptor type */
#define GVE_TX_TYPE_CONT 0x20

/**
 * Maximum number of receive buffers
 *
 * This is a policy decision.  Experiments suggest that using fewer
 * than 64 receive buffers leads to excessive packet drop rates on
 * some instance types.
 */
#define GVE_RX_FILL 64

/** Receive queue page list ID */
#define GVE_RX_QPL 0x18ae5258

/** Receive queue interrupt channel */
#define GVE_RX_IRQ 1

/** A receive descriptor */
struct gve_rx_descriptor_gqi {
	/** Buffer descriptor */
	struct gve_buffer buf;
} __attribute__ (( packed ));

/** A receive packet descriptor */
struct gve_rx_packet {
	/** Length */
	uint16_t len;
	/** Flags */
	uint8_t flags;
	/** Sequence number */
	uint8_t seq;
} __attribute__ (( packed ));

/** DQO TX packet descriptor */
struct gve_tx_descriptor_dqo {
	/** Buffer address */
	uint64_t buf_addr;

	/** Descriptor type. Must be GVE_TX_PKT_DESC_DTYPE_DQO (0xc) */
	uint8_t dtype: 5;

	/* Denotes the last descriptor of a packet. */
	uint8_t end_of_packet: 1;
	uint8_t checksum_offload_enable: 1;

	/* If set, will generate a descriptor completion for this descriptor. */
	uint8_t report_event: 1;
	uint8_t reserved0;
	uint16_t reserved1;

	/* The TX completion associated with this packet will contain this tag. */
	uint16_t compl_tag;
	uint16_t buf_size: 14;
	uint16_t reserved2: 2;
} __attribute__ (( packed ));


#define GVE_COMPL_TYPE_DQO_PKT 0x2 /* Packet completion */
#define GVE_COMPL_TYPE_DQO_DESC 0x4 /* Descriptor completion */
#define GVE_COMPL_TYPE_DQO_MISS 0x1 /* Miss path completion */
#define GVE_COMPL_TYPE_DQO_REINJECTION 0x3 /* Re-injection completion */


/** DQO TX completion descriptor */
struct gve_tx_completion_dqo {
	/* For types 0-4 this is the TX queue ID associated with this
	 * completion.
	 */
	uint16_t id: 11;

	/* See: GVE_COMPL_TYPE_DQO* */
	uint16_t type: 3;
	uint16_t reserved0: 1;

	/* Flipped by HW to notify the descriptor is populated. */
	uint16_t generation: 1;
	union {
		/* For descriptor completions, this is the last index fetched
		 * by HW + 1.
		 */
		uint16_t tx_head;

		/* For packet completions, this is the completion tag set on the
		 * TX packet descriptors.
		 */
		uint16_t completion_tag;
	};
	uint32_t reserved1;
} __attribute__ (( packed ));

/** Receive error */
#define GVE_RXF_ERROR 0x08

/** Receive packet continues into next descriptor */
#define GVE_RXF_MORE 0x20

/** Receive sequence number mask */
#define GVE_RX_SEQ_MASK 0x07

/** A receive completion descriptor */
struct gve_rx_completion_gqi {
	/** Reserved */
	uint8_t reserved[60];
	/** Packet descriptor */
	struct gve_rx_packet pkt;
} __attribute__ (( packed ));

/* Descriptor to post buffers to HW on buffer queue. */
struct gve_rx_descriptor_dqo {
	uint16_t buf_id; /* ID returned in Rx completion descriptor */
	uint16_t reserved0;
	uint32_t reserved1;
	uint64_t buf_addr; /* DMA address of the buffer */
	uint64_t header_buf_addr;
	uint64_t reserved2;
} __attribute__ (( packed ));

/* Descriptor for HW to notify SW of new packets received on RX queue. */
struct gve_rx_completion_dqo {
	/* Must be 1 */
	uint8_t rxdid: 4;
	uint8_t reserved0: 4;

	/* Packet originated from this system rather than the network. */
	uint8_t loopback: 1;
	/* Set when IPv6 packet contains a destination options header or routing
	 * header.
	 */
	uint8_t ipv6_ex_add: 1;
	/* Invalid packet was received. */
	uint8_t rx_error: 1;
	uint8_t reserved1: 5;

	uint16_t packet_type: 10;
	uint16_t ip_hdr_err: 1;
	uint16_t udp_len_err: 1;
	uint16_t raw_cs_invalid: 1;
	uint16_t reserved2: 3;

	uint16_t packet_len: 14;
	/* Flipped by HW to notify the descriptor is populated. */
	uint16_t generation: 1;
	/* Should be zero. */
	uint16_t buffer_queue_id: 1;

	uint16_t reserved3;

	uint8_t descriptor_done: 1;
	uint8_t reserved4: 2;
	uint8_t l3_l4_processed: 1;
	uint8_t csum_ip_err: 1;
	uint8_t csum_l4_err: 1;
	uint8_t csum_external_ip_err: 1;
	uint8_t csum_external_udp_err: 1;

	uint8_t status_error1;

	uint16_t reserved5;
	uint16_t buf_id; /* Buffer ID which was sent on the buffer queue. */

	uint16_t reserved6;
	uint32_t hash;
	uint8_t reserved7[12];
} __attribute__ (( packed ));


/** DQO device options */
enum gve_dev_opt_id {
	GVE_DEV_OPT_ID_GQI_RAW_ADDRESSING = 0x1,
	GVE_DEV_OPT_ID_GQI_RDA = 0x2,
	GVE_DEV_OPT_ID_GQI_QPL = 0x3,
	GVE_DEV_OPT_ID_DQO_RDA = 0x4,
	GVE_DEV_OPT_ID_DQO_QPL = 0x7
};

/** Padding at the start of all received packets */
#define GVE_RX_PAD 2

/** A descriptor queue */
struct gve_queue {
	/** Descriptor ring */
	union {
		/** Transmit descriptors */
		struct gve_tx_descriptor_gqi *gqi_tx_desc;
		/** Receive descriptors */
		struct gve_rx_descriptor_gqi *gqi_rx_desc;
		/** DQO transmit packet descriptors */
		struct gve_tx_descriptor_dqo *dqo_tx_desc;
		/** DQO receive buffer descriptors */
		struct gve_rx_descriptor_dqo *dqo_rx_buf;
		/** Raw data */
		void *raw;
	} desc;

	/** Completion ring */
	union {
		/** Receive completions */
		struct gve_rx_completion_gqi *gqi_rx;
		/** DQO transmit completions */
		struct gve_tx_completion_dqo *dqo_tx;
		/** DQO receive completion descriptors */
		struct gve_rx_completion_dqo *dqo_rx;
		/** Raw data */
		void *raw;
	} cmplt;

	/** Buffer for data packets */
	union {
		/** Queue page list */
		struct gve_qpl qpl;
		/** Buffer for RDA Rx data packets */
		struct gve_packet rx_buf;
	} buffer;

	/** Queue resources */
	struct gve_resources *res;

	/** Queue type */
	const struct gve_queue_type *type;
	/** Number of descriptors (must be a power of two) */
	unsigned int count;
	/** Maximum fill level (must be a power of two) */
	unsigned int fill;

	/** Descriptor mapping */
	struct dma_mapping desc_map;
	/** Completion mapping */
	struct dma_mapping cmplt_map;
	/** Queue resources mapping */
	struct dma_mapping res_map;
	/** Rx buffer queue mapping */
	struct dma_mapping rxbuf_map[1024];

	/** Doorbell register */
	volatile uint32_t *db;
	/** Event counter */
	struct gve_event *event;

	/** Producer counter */
	uint32_t prod;
	/** Consumer counter */
	uint32_t cons;

	/* Tracks the current gen bit of compl_q */
	uint8_t cur_gen_bit;
	uint32_t cmptl_counter;
};

/** A descriptor queue type */
struct gve_queue_type {
	/** Name */
	const char *name;
	/**
	 * Populate command parameters to create queue
	 *
	 * @v gve		GVE device
	 * @v queue		Descriptor queue
	 * @v cmd		Admin queue command
	 */
	void ( * param ) ( struct gve_nic *gve, struct gve_queue *queue,
			   union gve_admin_command *cmd );
	/** Queue page list ID */
	uint32_t qpl;
	/** Interrupt channel */
	uint8_t irq;
	/** Maximum fill level */
	uint8_t fill;
	/** GQ descriptor size */
	uint8_t gqi_desc_len;
	/** GQ completion size */
	uint8_t gqi_cmplt_len;
	/** DQO descriptor size */
	uint8_t dqo_desc_len;
	/** DQO dcompletion size */
	uint8_t dqo_cmplt_len;
	/** Command to create queue */
	uint8_t create;
	/** Command to destroy queue */
	uint8_t destroy;
};

/** A Google Virtual Ethernet NIC */
struct gve_nic {
	/** Configuration registers */
	void *cfg;
	/** Doorbell registers */
	void *db;
	/** PCI revision */
	uint8_t revision;
	/** Queue format */
	enum gve_queue_format queue_format;
	/** Network device */
	struct net_device *netdev;
	/** DMA device */
	struct dma_device *dma;

	/** Admin queue */
	struct gve_admin admin;
	/** Interrupt channels */
	struct gve_irqs irqs;
	/** Event counters */
	struct gve_events events;
	/** Scratch buffer */
	struct gve_scratch scratch;

	/** Transmit queue */
	struct gve_queue tx;
	/** Receive queue */
	struct gve_queue rx;
	/** Transmit I/O buffers */
	struct io_buffer *tx_iobuf[GVE_TX_FILL];
	/** Receive sequence number */
	unsigned int seq;

	/** Startup process */
	struct process startup;
	/** Startup process retry counter */
	unsigned int retries;
	/** Reset recovery watchdog timer */
	struct retry_timer watchdog;
	/** Reset recovery recorded activity counter */
	uint32_t activity;
};

/** Maximum time to wait for admin queue commands */
#define GVE_ADMIN_MAX_WAIT_MS 500

/** Maximum number of times to reattempt device reset */
#define GVE_RESET_MAX_RETRY 5

/** Time between reset recovery checks */
#define GVE_WATCHDOG_TIMEOUT ( 1 * TICKS_PER_SEC )

#endif /* _GVE_H */
